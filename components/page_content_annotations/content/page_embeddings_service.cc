// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_embeddings_service.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <utility>

#include "components/page_content_annotations/content/embeddings_candidate_generator.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace page_content_annotations {

namespace {
passage_embeddings::PassagePriority ConvertToPassagePriority(
    PageEmbeddingsService::Priority priority) {
  switch (priority) {
    case PageEmbeddingsService::kUserBlocking:
      return passage_embeddings::kUserInitiated;

    case PageEmbeddingsService::kUrgent:
      return passage_embeddings::kUrgent;

    case PageEmbeddingsService::kDefault:
      return passage_embeddings::kPassive;

    case PageEmbeddingsService::kBackground:
      return passage_embeddings::kLatent;
  }
}
}  // namespace

PassageEmbedding::PassageEmbedding() = default;
PassageEmbedding::~PassageEmbedding() = default;
PassageEmbedding::PassageEmbedding(const PassageEmbedding& other) = default;
PassageEmbedding::PassageEmbedding(
    std::pair<std::string, EmbeddingPassageType> passage,
    passage_embeddings::Embedding embedding)
    : passage(std::move(passage)), embedding(std::move(embedding)) {}

struct PageEmbeddingsService::PageState {
  explicit PageState(content::Page& page) : page(page.GetWeakPtr()) {}

  base::WeakPtr<content::Page> page;

  // pending_passages is non-empty from the time passages are produced via
  // candidates_generator_ to the time that embeddings are requested.
  std::vector<std::pair<std::string, EmbeddingPassageType>> pending_passages;

  // The currently active task for computing embeddings. Non-empty while the
  // embedding computation is pending.
  std::optional<passage_embeddings::Embedder::TaskId> active_task;

  // passage_embeddings is empty until embeddings are received.
  std::vector<PassageEmbedding> passage_embeddings;
};

struct PageEmbeddingsService::WebContentsState {
  std::unique_ptr<WebContentsEventsObserver> observer;

  std::optional<PageState> page_state;
};

class PageEmbeddingsService::WebContentsEventsObserver
    : public content::WebContentsObserver {
 public:
  WebContentsEventsObserver(content::WebContents* web_contents,
                            PageEmbeddingsService* page_embeddings_service)
      : WebContentsObserver(web_contents),
        page_embeddings_service_(page_embeddings_service) {}
  ~WebContentsEventsObserver() override = default;

  void OnVisibilityChanged(content::Visibility visibility) override {
    if (visibility == content::Visibility::HIDDEN) {
      page_embeddings_service_->ComputeEmbeddingsOnHide(
          web_contents()->GetPrimaryPage());
    }
  }

  void WebContentsDestroyed() override {
    page_embeddings_service_->web_contents_state_.erase(web_contents());
  }

  bool IsWebContentsHidden() const {
    return web_contents()->GetVisibility() == content::Visibility::HIDDEN;
  }

 private:
  raw_ptr<PageEmbeddingsService> page_embeddings_service_;
};

PageEmbeddingsService::Priority
PageEmbeddingsService::Observer::GetDefaultPriority() const {
  return kDefault;
}

PageEmbeddingsService::UsageMode PageEmbeddingsService::Observer::GetUsageMode()
    const {
  return kOnDemand;
}

PageEmbeddingsService::ScopedPriority::ScopedPriority(
    PageEmbeddingsService* service,
    Observer* observer,
    Priority priority)
    : service_(service), observer_(observer) {
  // Only one scoped priority per observer is supported.
  DCHECK_EQ(0u, service_->temporary_priority_.count(observer));

  // We only support raising the priority.
  DCHECK_LT(priority, observer->GetDefaultPriority());

  service_->temporary_priority_[observer] = priority;

  if (priority < service_->current_priority_) {
    service_->current_priority_ = priority;
    service_->UpdateTaskPriorities(service_->current_priority_);
  }
}

PageEmbeddingsService::ScopedPriority::~ScopedPriority() {
  if (!service_) {
    // The object has been moved-from.
    return;
  }

  service_->temporary_priority_.erase(observer_);

  Priority next_priority =
      GetActivePriority(service_->observers_, service_->temporary_priority_);
  if (next_priority != service_->current_priority_) {
    service_->current_priority_ = next_priority;
    service_->UpdateTaskPriorities(service_->current_priority_);
  }
}

PageEmbeddingsService::ScopedPriority::ScopedPriority(ScopedPriority&& other) {
  *this = std::move(other);
}

PageEmbeddingsService::ScopedPriority&
PageEmbeddingsService::ScopedPriority::operator=(ScopedPriority&& other) {
  service_ = other.service_;
  observer_ = other.observer_;

  other.service_ = nullptr;
  other.observer_ = nullptr;

  return *this;
}

PageEmbeddingsService::PageEmbeddingsService(
    EmbeddingCandidatesGenerator candidates_generator,
    PageContentExtractionService* page_content_extraction_service,
    passage_embeddings::Embedder* embedder)
    : candidates_generator_(candidates_generator),
      embedder_(embedder),
      page_content_extraction_service_(page_content_extraction_service) {}

PageEmbeddingsService::PageEmbeddingsService(
    PageContentExtractionService* page_content_extraction_service)
    : PageEmbeddingsService(base::BindRepeating(&GenerateEmbeddingsCandidates),
                            page_content_extraction_service,
                            nullptr) {}

PageEmbeddingsService::~PageEmbeddingsService() = default;

void PageEmbeddingsService::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);

  if (!page_content_extraction_observation_.IsObserving()) {
    page_content_extraction_observation_.Observe(
        page_content_extraction_service_);
  }

  UpdateTaskPriorities(GetActivePriority(observers_, temporary_priority_));

  if (const UsageMode next_usage_mode = observer->GetUsageMode();
      next_usage_mode > current_usage_mode_) {
    if (next_usage_mode == kContinuous) {
      // Compute embeddings eagerly for all foreground tabs, to ensure that they
      // are available.
      for (const auto& [web_contents, web_contents_state] :
           web_contents_state_) {
        if (web_contents_state.page_state &&
            web_contents_state.page_state->page &&
            !web_contents_state.observer->IsWebContentsHidden()) {
          ComputeEmbeddings(*web_contents_state.page_state->page);
        }
      }
    }
    current_usage_mode_ = next_usage_mode;
  }
}

void PageEmbeddingsService::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);

  if (observers_.empty() &&
      page_content_extraction_observation_.IsObserving()) {
    page_content_extraction_observation_.Reset();
  }

  UpdateTaskPriorities(GetActivePriority(observers_, temporary_priority_));
  current_usage_mode_ = GetActiveUsageMode(observers_);
}

PageEmbeddingsService::ScopedPriority PageEmbeddingsService::RaisePriority(
    Observer* observer,
    Priority priority) {
  return ScopedPriority(this, observer, priority);
}

void PageEmbeddingsService::ProcessEmbeddingsOnDemand() {
  if (current_usage_mode_ == kContinuous) {
    // In continuous mode all tab embeddings are computed eagerly, so there's
    // nothing to do.
    return;
  }

  // Force the computation of embeddings for all visible tabs, which are
  // otherwise only lazily computed on being hidden.
  for (const auto& [web_contents, web_contents_state] : web_contents_state_) {
    if (web_contents_state.page_state && web_contents_state.page_state->page &&
        !web_contents_state.observer->IsWebContentsHidden()) {
      ComputeEmbeddings(*web_contents_state.page_state->page);
    }
  }
}

std::vector<PassageEmbedding> PageEmbeddingsService::GetEmbeddings(
    content::Page& page) const {
  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  const auto loc = web_contents_state_.find(web_contents);
  if (loc == web_contents_state_.end() || !loc->second.page_state ||
      loc->second.page_state->page.get() != &page) {
    return {};
  }
  return loc->second.page_state->passage_embeddings;
}

void PageEmbeddingsService::OnPageContentExtracted(
    content::Page& page,
    scoped_refptr<const RefCountedAnnotatedPageContent> page_content) {
  CHECK(page_content);
  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());

  auto loc = web_contents_state_.find(web_contents);
  if (loc == web_contents_state_.end()) {
    loc = web_contents_state_.try_emplace(web_contents).first;
    loc->second.observer =
        std::make_unique<WebContentsEventsObserver>(web_contents, this);
  }

  WebContentsState& state = loc->second;
  if (state.page_state) {
    if (state.page_state->active_task.has_value()) {
      embedder_->TryCancel(*state.page_state->active_task);
    }
    state.page_state.reset();
  }

  state.page_state.emplace(page);

  state.page_state->pending_passages = candidates_generator_.Run(
      page_content->data, passage_embeddings::kMaxPassagesPerPage.Get());

  if (current_usage_mode_ == kContinuous ||
      state.observer->IsWebContentsHidden()) {
    // The WebContents may have transitioned from visible to hidden by the time
    // we received the passages, so compute embeddings.
    ComputeEmbeddings(page);
  }
}

void PageEmbeddingsService::ComputeEmbeddings(content::Page& page) {
  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  auto loc = web_contents_state_.find(web_contents);
  if (loc == web_contents_state_.end() || !loc->second.page_state ||
      loc->second.page_state->page.get() != &page) {
    return;
  }

  WebContentsState& state = loc->second;
  if (state.page_state->active_task.has_value()) {
    embedder_->TryCancel(*state.page_state->active_task);
    state.page_state->active_task.reset();
  }

  if (state.page_state->pending_passages.empty()) {
    return;
  }

  // Ensure that state.page_state->pending_passages is cleared before invoking
  // ComputePassagesEmbeddings().
  std::vector<std::pair<std::string, EmbeddingPassageType>> pending_passages;
  pending_passages.swap(state.page_state->pending_passages);

  std::vector<EmbeddingPassageType> passage_types;
  passage_types.reserve(pending_passages.size());
  std::vector<std::string> string_passages;
  string_passages.reserve(pending_passages.size());
  for (const auto& passage : pending_passages) {
    string_passages.push_back(passage.first);
    passage_types.push_back(passage.second);
  }

  state.page_state->active_task = embedder_->ComputePassagesEmbeddings(
      ConvertToPassagePriority(current_priority_), std::move(string_passages),
      base::BindOnce(&PageEmbeddingsService::OnEmbeddingsComputed,
                     weak_ptr_factory_.GetWeakPtr(), std::move(passage_types),
                     web_contents->GetWeakPtr(), state.page_state->page));
}

void PageEmbeddingsService::ComputeEmbeddingsOnHide(content::Page& page) {
  if (current_usage_mode_ != kOnDemand) {
    return;
  }

  ComputeEmbeddings(page);
}

void PageEmbeddingsService::OnEmbeddingsComputed(
    std::vector<EmbeddingPassageType> passage_types,
    base::WeakPtr<content::WebContents> web_contents,
    base::WeakPtr<content::Page> page,
    std::vector<std::string> passage_strings,
    std::vector<passage_embeddings::Embedding> embeddings,
    passage_embeddings::Embedder::TaskId task_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  if (!web_contents) {
    // The web contents was destroyed while computing the embeddings.
    return;
  }

  if (!page) {
    // The page was destroyed while computing the embeddings.
    const auto loc = web_contents_state_.find(web_contents.get());
    if (loc != web_contents_state_.end() && loc->second.page_state &&
        !loc->second.page_state->page) {
      loc->second.page_state.reset();
    }
    return;
  }

  const auto loc = web_contents_state_.find(web_contents.get());
  DCHECK(loc != web_contents_state_.end());

  // Ignore stale embeddings from previously cancelled tasks or old pages.
  if (!loc->second.page_state ||
      loc->second.page_state->page.get() != page.get() ||
      loc->second.page_state->active_task != task_id) {
    return;
  }

  loc->second.page_state->active_task.reset();
  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
    loc->second.page_state->passage_embeddings.clear();
    return;
  }

  CHECK_EQ(passage_types.size(), embeddings.size());
  CHECK_EQ(passage_strings.size(), embeddings.size());

  std::vector<PassageEmbedding> passage_embeddings;
  for (size_t i = 0; i < passage_types.size(); ++i) {
    passage_embeddings.emplace_back(
        std::make_pair(std::move(passage_strings[i]),
                       std::move(passage_types[i])),
        std::move(embeddings[i]));
  }
  loc->second.page_state->passage_embeddings = std::move(passage_embeddings);

  for (Observer& observer : observers_) {
    observer.OnPageEmbeddingsAvailable(*page);
  }
}

// static
PageEmbeddingsService::Priority PageEmbeddingsService::GetActivePriority(
    const base::ObserverList<Observer>& observers,
    const std::map<Observer*, Priority>& temporary_priority) {
  const Priority highest_default_priority = std::transform_reduce(
      observers.begin(), observers.end(), kDefault,
      [](Priority p1, Priority p2) { return std::min(p1, p2); },
      [](const Observer& observer) { return observer.GetDefaultPriority(); });

  return std::transform_reduce(
      temporary_priority.begin(), temporary_priority.end(),
      highest_default_priority,
      [](Priority p1, Priority p2) { return std::min(p1, p2); },
      [](const std::map<Observer*, Priority>::value_type& pair) {
        return pair.second;
      });
}

void PageEmbeddingsService::UpdateTaskPriorities(Priority priority) {
  if (priority == current_priority_) {
    return;
  }

  current_priority_ = priority;

  std::set<passage_embeddings::Embedder::TaskId> tasks;
  for (const auto& [web_contents, web_contents_state] : web_contents_state_) {
    if (web_contents_state.page_state &&
        web_contents_state.page_state->active_task.has_value()) {
      tasks.insert(*web_contents_state.page_state->active_task);
    }
  }

  if (!tasks.empty()) {
    embedder_->ReprioritizeTasks(ConvertToPassagePriority(priority), tasks);
  }
}

// static
PageEmbeddingsService::UsageMode PageEmbeddingsService::GetActiveUsageMode(
    const base::ObserverList<Observer>& observers) {
  return std::transform_reduce(
      observers.begin(), observers.end(), kOnDemand,
      [](UsageMode u1, UsageMode u2) { return std::max(u1, u2); },
      [](const Observer& observer) { return observer.GetUsageMode(); });
}

}  // namespace page_content_annotations
