// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/page_content_annotations/content/page_embeddings_service.h"

#include <algorithm>
#include <numeric>
#include <set>
#include <string_view>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "components/page_content_annotations/content/embeddings_candidate_generator.h"
#include "components/page_content_annotations/content/page_content_extraction_service.h"
#include "components/passage_embeddings/core/passage_embeddings_features.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

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

// LINT.IfChange(PageEmbeddingsPriority)
std::string_view PageEmbeddingsPriorityToString(
    PageEmbeddingsService::Priority priority) {
  switch (priority) {
    case PageEmbeddingsService::kUserBlocking:
      return "UserBlocking";
    case PageEmbeddingsService::kUrgent:
      return "Urgent";
    case PageEmbeddingsService::kDefault:
      return "Default";
    case PageEmbeddingsService::kBackground:
      return "Background";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/histograms.xml:PageEmbeddingsPriority)

// LINT.IfChange(PageEmbeddingsUsageMode)
std::string_view PageEmbeddingsUsageModeToString(
    PageEmbeddingsService::UsageMode usage_mode) {
  switch (usage_mode) {
    case PageEmbeddingsService::kOnDemand:
      return "OnDemand";
    case PageEmbeddingsService::kContinuous:
      return "Continuous";
  }
}
// LINT.ThenChange(//tools/metrics/histograms/metadata/optimization/histograms.xml:PageEmbeddingsUsageMode)

}  // namespace

// Passages have been produced for the page, but embedding computation has not
// yet started.
struct PageEmbeddingsService::Pending {
  std::vector<std::pair<std::string, EmbeddingPassageType>> passages;
  base::TimeTicks queue_time = base::TimeTicks::Now();
};

// Embedding computation is currently in progress for the page.
struct PageEmbeddingsService::Computing {
  passage_embeddings::Embedder::Job job;
  base::TimeTicks queue_time;
  Priority priority;
};

// Embeddings have been successfully computed for the page and are available.
struct PageEmbeddingsService::Available {
  std::vector<PassageEmbedding> embeddings;
};

// No embeddings could be produced for the page (e.g. no passages were found, or
// the computation failed).
struct PageEmbeddingsService::Unavailable {};

struct PageEmbeddingsService::WebContentsState {
  using EmbeddingsState =
      std::variant<Unavailable, Pending, Computing, Available>;

  WebContentsState() = default;
  ~WebContentsState() = default;

  std::unique_ptr<WebContentsEventsObserver> observer;

  base::WeakPtr<content::Page> page;
  EmbeddingsState embeddings_state = Unavailable{};
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

  void PrimaryPageChanged(content::Page& page) override {
    auto loc =
        page_embeddings_service_->web_contents_states_.find(web_contents());
    if (loc != page_embeddings_service_->web_contents_states_.end()) {
      loc->second.page = nullptr;
      loc->second.embeddings_state = Unavailable{};
    }
  }

  void WebContentsDestroyed() override {
    page_embeddings_service_->web_contents_states_.erase(web_contents());
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
    passage_embeddings::Embedder* embedder,
    passage_embeddings::EmbedderMetadataProvider* embedder_metadata_provider)
    : candidates_generator_(candidates_generator),
      embedder_(embedder),
      embedder_metadata_provider_(embedder_metadata_provider),
      page_content_extraction_service_(page_content_extraction_service) {}

PageEmbeddingsService::PageEmbeddingsService(
    PageContentExtractionService* page_content_extraction_service)
    : PageEmbeddingsService(base::BindRepeating(&GenerateEmbeddingsCandidates),
                            page_content_extraction_service,
                            /*embedder=*/nullptr,
                            /*embedder_metadata_provider=*/nullptr) {}

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
           web_contents_states_) {
        if (web_contents_state.page &&
            !web_contents_state.observer->IsWebContentsHidden() &&
            std::holds_alternative<Pending>(
                web_contents_state.embeddings_state)) {
          ComputeEmbeddings(*web_contents_state.page);
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
  for (const auto& [web_contents, web_contents_state] : web_contents_states_) {
    if (web_contents_state.page &&
        !web_contents_state.observer->IsWebContentsHidden() &&
        std::holds_alternative<Pending>(web_contents_state.embeddings_state)) {
      ComputeEmbeddings(*web_contents_state.page);
    }
  }
}

std::vector<PassageEmbedding> PageEmbeddingsService::GetEmbeddings(
    content::Page& page) const {
  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  const auto loc = web_contents_states_.find(web_contents);
  if (loc == web_contents_states_.end() || loc->second.page.get() != &page) {
    return {};
  }
  if (auto* available = std::get_if<Available>(&loc->second.embeddings_state)) {
    return available->embeddings;
  }
  return {};
}

passage_embeddings::EmbedderMetadataProvider*
PageEmbeddingsService::GetEmbedderMetadataProvider() {
  return embedder_metadata_provider_;
}

void PageEmbeddingsService::OnPageContentExtracted(content::Page& page,
                                                   PageContent page_content) {
  if (!IsPageContentValid(page_content)) {
    return;
  }

  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());

  auto loc = web_contents_states_.find(web_contents);
  if (loc == web_contents_states_.end()) {
    loc = web_contents_states_.try_emplace(web_contents).first;
    loc->second.observer =
        std::make_unique<WebContentsEventsObserver>(web_contents, this);
  }

  WebContentsState& state = loc->second;
  if (std::holds_alternative<Computing>(state.embeddings_state)) {
    state.embeddings_state = Unavailable{};
  }

  state.page = page.GetWeakPtr();

  std::vector<std::pair<std::string, EmbeddingPassageType>> pending_passages =
      candidates_generator_.Run(
          page_content,
          std::visit(absl::Overload{
                         [](RefCountedAnnotatedPageContentPtr) {
                           return passage_embeddings::kMaxPassagesPerPage.Get();
                         },
                         [](RefCountedPDFTextPtr) {
                           return passage_embeddings::kMaxPassagesFromPDF.Get();
                         }},
                     page_content),
          base::UTF16ToUTF8(web_contents->GetTitle()),
          web_contents->GetLastCommittedURL().spec());

  if (!pending_passages.empty()) {
    state.embeddings_state = Pending{.passages = std::move(pending_passages)};

    if (current_usage_mode_ == kContinuous ||
        state.observer->IsWebContentsHidden()) {
      // The WebContents may have transitioned from visible to hidden by the
      // time we received the passages, so compute embeddings.
      ComputeEmbeddings(page);
    }
  } else {
    state.embeddings_state = Unavailable{};
  }
}

void PageEmbeddingsService::ComputeEmbeddings(content::Page& page) {
  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  auto loc = web_contents_states_.find(web_contents);
  if (loc == web_contents_states_.end() || loc->second.page.get() != &page) {
    return;
  }

  WebContentsState& state = loc->second;
  auto* pending = std::get_if<Pending>(&state.embeddings_state);
  CHECK(pending);

  std::vector<std::pair<std::string, EmbeddingPassageType>> passages =
      std::move(pending->passages);
  base::TimeTicks queue_time = pending->queue_time;

  std::vector<EmbeddingPassageType> passage_types;
  passage_types.reserve(passages.size());
  std::vector<std::string> string_passages;
  string_passages.reserve(passages.size());
  for (const auto& passage : passages) {
    string_passages.push_back(passage.first);
    passage_types.push_back(passage.second);
  }

  state.embeddings_state =
      Computing{.job = embedder_->ComputePassagesEmbeddings(
                    ConvertToPassagePriority(current_priority_),
                    std::move(string_passages),
                    base::BindOnce(&PageEmbeddingsService::OnEmbeddingsComputed,
                                   weak_ptr_factory_.GetWeakPtr(),
                                   std::move(passage_types),
                                   web_contents->GetWeakPtr(), state.page)),
                .queue_time = queue_time,
                .priority = current_priority_};
}

void PageEmbeddingsService::ComputeEmbeddingsOnHide(content::Page& page) {
  if (current_usage_mode_ != kOnDemand) {
    return;
  }

  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  const auto loc = web_contents_states_.find(web_contents);
  if (loc != web_contents_states_.end() && loc->second.page.get() == &page &&
      std::holds_alternative<Pending>(loc->second.embeddings_state)) {
    ComputeEmbeddings(page);
  }
}

void PageEmbeddingsService::OnEmbeddingsComputed(
    std::vector<EmbeddingPassageType> passage_types,
    base::WeakPtr<content::WebContents> web_contents,
    base::WeakPtr<content::Page> page,
    std::vector<std::string> passage_strings,
    std::vector<passage_embeddings::Embedding> embeddings,
    uint64_t job_id,
    passage_embeddings::ComputeEmbeddingsStatus status) {
  if (!web_contents) {
    // The web contents was destroyed while computing the embeddings.
    return;
  }

  if (!page) {
    // The page was destroyed while computing the embeddings.
    const auto loc = web_contents_states_.find(web_contents.get());
    if (loc != web_contents_states_.end() && !loc->second.page) {
      loc->second.embeddings_state = Unavailable{};
    }
    return;
  }

  const auto loc = web_contents_states_.find(web_contents.get());
  DCHECK(loc != web_contents_states_.end());

  // Ignore stale embeddings from previously cancelled jobs or old pages.
  if (loc->second.page.get() != page.get()) {
    return;
  }

  auto* computing = std::get_if<Computing>(&loc->second.embeddings_state);
  if (!computing || computing->job.id() != job_id) {
    return;
  }

  if (status != passage_embeddings::ComputeEmbeddingsStatus::kSuccess) {
    loc->second.embeddings_state = Unavailable{};
    return;
  }

  base::UmaHistogramLongTimes(
      base::StrCat({"OptimizationGuide.PageEmbeddings.Job.TotalDuration.",
                    PageEmbeddingsUsageModeToString(current_usage_mode_), ".",
                    PageEmbeddingsPriorityToString(computing->priority)}),
      base::TimeTicks::Now() - computing->queue_time);

  CHECK_EQ(passage_types.size(), embeddings.size());
  CHECK_EQ(passage_strings.size(), embeddings.size());

  std::vector<PassageEmbedding> passage_embeddings;
  passage_embeddings.reserve(passage_types.size());
  for (size_t i = 0; i < passage_types.size(); ++i) {
    passage_embeddings.emplace_back(
        std::make_pair(std::move(passage_strings[i]),
                       std::move(passage_types[i])),
        std::move(embeddings[i]));
  }
  loc->second.embeddings_state =
      Available{.embeddings = std::move(passage_embeddings)};

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
  if (current_priority_ == priority) {
    return;
  }

  current_priority_ = priority;

  std::set<uint64_t> job_ids;
  for (const auto& [web_contents, web_contents_state] : web_contents_states_) {
    if (auto* computing =
            std::get_if<Computing>(&web_contents_state.embeddings_state)) {
      job_ids.insert(computing->job.id());
    }
  }

  if (!job_ids.empty()) {
    embedder_->ReprioritizeJobs(ConvertToPassagePriority(priority), job_ids);
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
