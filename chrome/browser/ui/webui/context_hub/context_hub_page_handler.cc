// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/context_hub/context_hub_page_handler.h"

#include <algorithm>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/barrier_callback.h"
#include "base/barrier_closure.h"
#include "base/check.h"
#include "base/containers/adapters.h"
#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_params.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "base/version_info/channel.h"
#include "base/version_info/version_info.h"
#include "build/build_config.h"
#include "chrome/browser/context_hub/auto_todos/auto_todo_entry.h"
#include "chrome/browser/context_hub/context_hub_service.h"
#include "chrome/browser/context_hub/context_hub_service_factory.h"
#include "chrome/browser/context_hub/features.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_chat_types.h"
#include "chrome/browser/context_hub/memory_bank/memory_bank_entry.h"
#include "chrome/browser/context_hub/topics/topics_feedback_exporter.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/context_hub/context_hub.mojom-features.h"
#include "chrome/common/channel_info.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon_base/fallback_icon_style.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history/core/browser/journeys/journey.h"
#include "components/history/core/browser/journeys/journey_row.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/page_image_service/image_service.h"
#include "components/page_image_service/mojom/page_image_service.mojom.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/message.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/clipboard/file_info.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "url/gurl.h"

#if !BUILDFLAG(IS_ANDROID)
#include "base/base_paths.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/file_util_icu.h"
#include "base/path_service.h"
#include "base/task/lazy_thread_pool_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/context_hub/record_replay/skill_recording_service.h"
#include "chrome/browser/context_hub/record_replay/skill_recording_service_factory.h"
#include "chrome/browser/glic/host/context/glic_sharing_utils.h"  // nogncheck
#include "chrome/browser/glic/host/glic.mojom.h"                  // nogncheck
#include "chrome/browser/glic/public/glic_enabling.h"             // nogncheck
#include "chrome/browser/glic/public/glic_instance.h"             // nogncheck
#include "chrome/browser/glic/public/glic_invoke_options.h"       // nogncheck
#include "chrome/browser/glic/public/glic_keyed_service.h"        // nogncheck
#include "chrome/browser/glic/public/glic_passkeys.h"             // nogncheck
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/browser/ui/webui/context_hub/context_hub_tab_provider_desktop.h"
#include "chrome/common/webui_url_constants.h"
#include "components/tabs/public/tab_interface.h"  // nogncheck
#include "content/public/browser/page_navigator.h"
#include "net/base/url_util.h"  // nogncheck
#endif
#include "components/sessions/content/session_tab_helper.h"  // nogncheck

#if !BUILDFLAG(IS_ANDROID)
namespace {

// Subdirectory in the system temp directory used to stage clipboard file
// attachments. Staged batches (>1 hour old) are pruned when the UI opens
// or on subsequent copy operations, with OS temp cleanup as a fallback.
constexpr base::FilePath::CharType kMemoryBanksTempDir[] =
    FILE_PATH_LITERAL("chrome_memory_banks");

// All disk writes and pruning across all browser windows are queued onto a
// single sequenced task runner to prevent concurrent filesystem races in the
// shared staging directory.
base::LazyThreadPoolSequencedTaskRunner g_clipboard_file_task_runner =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        base::TaskTraits(base::MayBlock(), base::TaskPriority::USER_VISIBLE));

scoped_refptr<base::SequencedTaskRunner> GetClipboardFileTaskRunner() {
  return g_clipboard_file_task_runner.Get();
}

// Resource limits for CopyAttachmentsToClipboard to prevent renderer-induced
// resource exhaustion.
constexpr size_t kMaxFilesToCopy = 10;
constexpr size_t kMaxFileContentBytes = 10 * 1024 * 1024;  // 10 MB
// Maximum filename length accepted over Mojo (matches filesystem NAME_MAX).
constexpr size_t kMaxFilenameLength = 255;
// Truncation limit for staged filenames to ensure full paths remain safely
// within the Windows MAX_PATH limit across all platforms.
constexpr size_t kMaxStagedFilenameLength = 128;
// Maximum aggregate content size across all files in a batch to bound browser
// memory usage and UI thread validation work.
constexpr size_t kMaxTotalContentBytes = 10 * 1024 * 1024;  // 10 MB

// Canonicalizes filenames to a standardized folded case for deduplication,
// ensuring files that differ only by case (e.g., "Notes.md" and "NOTES.md")
// do not collide on case-insensitive filesystems (such as Windows and macOS).
std::string CanonicalizeFilenameForDeduplication(const std::string& filename) {
  return base::UTF16ToUTF8(base::i18n::FoldCase(base::UTF8ToUTF16(filename)));
}

std::string SanitizeAttachmentFilename(std::string filename) {
  base::FilePath::StringType sanitized_path =
      base::FilePath::FromUTF8Unsafe(filename).value();

  // Replace illegal filesystem characters (including OS-reserved characters,
  // slashes, control chars, and illegal trailing/leading dots/spaces) while
  // preserving valid international multi-byte UTF-8 code points.
  base::i18n::ReplaceIllegalCharactersInPath(&sanitized_path, '_');

  std::string sanitized_name =
      base::FilePath(sanitized_path).BaseName().AsUTF8Unsafe();
  base::TrimWhitespaceASCII(sanitized_name, base::TRIM_ALL, &sanitized_name);

  // Strip .md extension if present so we can enforce length and re-append it.
  if (base::EndsWith(sanitized_name, ".md",
                     base::CompareCase::INSENSITIVE_ASCII)) {
    sanitized_name.resize(sanitized_name.size() - 3);
  }
  base::TrimWhitespaceASCII(sanitized_name, base::TRIM_ALL, &sanitized_name);

  // If empty or purely illegal characters replaced, fall back to "memory".
  if (sanitized_name.empty()) {
    sanitized_name = "memory";
  }

  // Enforce maximum staged filename length including .md suffix.
  static constexpr size_t kMaxStemLength = kMaxStagedFilenameLength - 3;
  if (sanitized_name.size() > kMaxStemLength) {
    base::TruncateUTF8ToByteSize(sanitized_name, kMaxStemLength,
                                 &sanitized_name);
    if (sanitized_name.empty()) {
      sanitized_name = "memory";
    }
  }

  sanitized_name += ".md";
  return sanitized_name;
}

bool ExceedsAttachmentResourceLimits(
    base::span<const browser::context_hub::mojom::MemoryBankFileAttachmentPtr>
        files) {
  size_t total_content_bytes = 0;
  for (const auto& file : files) {
    if (file->filename.size() > kMaxFilenameLength ||
        file->content.size() > kMaxFileContentBytes) {
      return true;
    }
    total_content_bytes += file->content.size();
  }
  return total_content_bytes > kMaxTotalContentBytes;
}

bool HasValidUtf8Strings(
    base::span<const browser::context_hub::mojom::MemoryBankFileAttachmentPtr>
        files) {
  return std::ranges::all_of(files, [](const auto& file) {
    return base::IsStringUTF8(file->filename) &&
           base::IsStringUTF8(file->content);
  });
}

// Sanitizes `filename` and ensures it is unique (case-insensitively) within
// `used_filenames`, appending a numeric suffix (e.g., "_2.md") on collision.
std::string GetUniqueSanitizedFilename(
    std::string filename,
    base::flat_set<std::string>& used_filenames) {
  std::string clean_name = SanitizeAttachmentFilename(std::move(filename));
  std::string unique_name = clean_name;
  bool inserted =
      used_filenames.insert(CanonicalizeFilenameForDeduplication(unique_name))
          .second;
  if (inserted) {
    return unique_name;
  }

  std::string stem = base::FilePath::FromUTF8Unsafe(clean_name)
                         .RemoveExtension()
                         .AsUTF8Unsafe();
  int suffix = 2;
  while (!inserted) {
    std::string suffix_str = base::StringPrintf("_%d.md", suffix++);
    size_t max_stem = kMaxStagedFilenameLength > suffix_str.size()
                          ? kMaxStagedFilenameLength - suffix_str.size()
                          : 0;
    unique_name = base::StrCat(
        {base::TruncateUTF8ToByteSize(stem, max_stem), suffix_str});
    inserted =
        used_filenames.insert(CanonicalizeFilenameForDeduplication(unique_name))
            .second;
  }
  return unique_name;
}

std::vector<std::pair<std::string, std::string>>
SanitizeAndDeduplicateAttachments(
    std::vector<browser::context_hub::mojom::MemoryBankFileAttachmentPtr>
        files) {
  base::flat_set<std::string> used_filenames;
  return base::ToVector(files, [&used_filenames](auto& file) {
    return std::make_pair(
        GetUniqueSanitizedFilename(std::move(file->filename), used_filenames),
        std::move(file->content));
  });
}

// Returns the clipboard staging root. Uses PathService rather than
// base::GetTempDir() so tests can redirect it with base::ScopedPathOverride.
bool GetStagingRootDir(base::FilePath* root_dir) {
  base::FilePath temp_dir;
  if (!base::PathService::Get(base::DIR_TEMP, &temp_dir)) {
    return false;
  }
  *root_dir = temp_dir.Append(kMemoryBanksTempDir);
  return true;
}

// Deletes batch directories older than 1 hour, bounding how long staged user
// content remains on disk while preserving recent clipboard operations. Must
// run on the sequence returned by GetClipboardFileTaskRunner().
void PruneStaleBatchesIn(const base::FilePath& root_dir) {
  CHECK(GetClipboardFileTaskRunner()->RunsTasksInCurrentSequence(),
        base::NotFatalUntil::M162);
  const base::Time cutoff_time = base::Time::Now() - base::Hours(1);
  base::FileEnumerator enumerator(root_dir, /*recursive=*/false,
                                  base::FileEnumerator::DIRECTORIES);
  for (base::FilePath entry = enumerator.Next(); !entry.empty();
       entry = enumerator.Next()) {
    if (enumerator.GetInfo().GetLastModifiedTime() < cutoff_time) {
      base::DeletePathRecursively(entry);
    }
  }
}

std::vector<std::pair<std::string, base::FilePath>> WriteAttachmentFilesToDisk(
    std::vector<std::pair<std::string, std::string>> files_to_write) {
  base::FilePath root_dir;
  if (!GetStagingRootDir(&root_dir) || !base::CreateDirectory(root_dir)) {
    return {};
  }

  PruneStaleBatchesIn(root_dir);

  // Create a unique subdirectory for this transaction to avoid races
  // and maintain clipboard persistence across successive copy
  // operations.
  base::FilePath target_dir;
  if (!base::CreateTemporaryDirInDir(root_dir, FILE_PATH_LITERAL("batch_"),
                                     &target_dir)) {
    return {};
  }
  std::vector<std::pair<std::string, base::FilePath>> written;
  written.reserve(files_to_write.size());
  for (const auto& [name, data] : files_to_write) {
    base::FilePath file_path =
        target_dir.Append(base::FilePath::FromUTF8Unsafe(name));
    if (!base::WriteFile(file_path, data)) {
      base::DeletePathRecursively(target_dir);
      return {};
    }
    written.emplace_back(name, file_path);
  }
  return written;
}

void PruneStaleClipboardBatchDirectories() {
  base::FilePath root_dir;
  // Unlike the write path, this must not create the staging root: there is
  // nothing to prune until the first copy happens.
  if (!GetStagingRootDir(&root_dir) || !base::DirectoryExists(root_dir)) {
    return;
  }
  PruneStaleBatchesIn(root_dir);
}

BrowserWindowInterface* GetBrowserWindowInterface(
    content::WebContents* web_contents,
    Profile* profile) {
  if (web_contents) {
    if (tabs::TabInterface* tab =
            tabs::TabInterface::MaybeGetFromContents(web_contents)) {
      if (BrowserWindowInterface* browser = tab->GetBrowserWindowInterface()) {
        return browser;
      }
    }
  }

  if (ProfileBrowserCollection* collection =
          ProfileBrowserCollection::GetForProfile(profile)) {
    return collection->GetLastActiveBrowser();
  }

  return nullptr;
}

}  // namespace
#endif

using history::journeys::Journey;
using history::journeys::JourneyContinuationQuery;
using history::journeys::JourneyVisit;
using history::journeys::JourneyVisitCollection;

ContextHubPageHandler::ContextHubPageHandler(
    mojo::PendingRemote<browser::context_hub::mojom::Page> page,
    mojo::PendingReceiver<browser::context_hub::mojom::PageHandler> receiver,
    Profile* profile,
    content::WebContents* web_contents,
    std::unique_ptr<TabProvider> tab_provider)
    : page_(std::move(page)),
      receiver_(this, std::move(receiver)),
      tab_provider_(std::move(tab_provider)),
      profile_(profile),
      web_contents_(web_contents) {
  CHECK(page_.is_bound());
  if (!tab_provider_) {
#if !BUILDFLAG(IS_ANDROID)
    tab_provider_ =
        std::make_unique<context_hub::ContextHubTabProviderDesktop>(profile_);
#endif
  }
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service_observation_.Observe(service);
    if (service->IsGeneratingFirstPartyAutoTodos()) {
      page_->OnFirstPartyAutoTodosGenerationStateChanged(true);
    }
  }
#if !BUILDFLAG(IS_ANDROID)
  // Proactively prune stale clipboard staging directories (>1 hour old)
  // created during prior sessions so temporary user notes do not linger in
  // the system temp directory until OS-level cleanup.
  GetClipboardFileTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&PruneStaleClipboardBatchDirectories));
#endif
}

ContextHubPageHandler::~ContextHubPageHandler() = default;

bool ContextHubPageHandler::TabProvider::OpenUrlsInTabGroup(
    const std::string& group_label,
    base::span<const GURL> urls) {
  return false;
}

void ContextHubPageHandler::TabProvider::OpenTopic(
    browser::context_hub::mojom::TopicIdOrUrlPtr topic_id_or_url) {}

#if !BUILDFLAG(IS_ANDROID)
GURL ContextHubPageHandler::TabProvider::ResolveTopicUrl(
    const browser::context_hub::mojom::TopicIdOrUrlPtr& topic_id_or_url) {
  if (!topic_id_or_url) {
    if (mojo::IsInMessageDispatch()) {
      mojo::ReportBadMessage("Missing topic_id_or_url");
    }
    return GURL();
  }

  GURL url;
  if (topic_id_or_url->is_topic_url()) {
    url = topic_id_or_url->get_topic_url();
  } else if (topic_id_or_url->is_topic_id()) {
    url = net::AppendQueryParameter(
        GURL(chrome::kChromeUIContextHubURL).Resolve("topic_details"), "id",
        topic_id_or_url->get_topic_id());
  }

  if (!url.is_valid() || !glic::IsContextHubTopicUrl(url)) {
    if (mojo::IsInMessageDispatch()) {
      mojo::ReportBadMessage("Invalid topic URL or host/path");
    }
    return GURL();
  }

  return url;
}
#endif

void ContextHubPageHandler::OnAutoTodosChanged(
    base::span<const context_hub::AutoTodoEntry> entries) {
  page_->OnAutoTodosChanged(
      std::vector<context_hub::AutoTodoEntry>(entries.begin(), entries.end()));
}

void ContextHubPageHandler::OnFirstPartyAutoTodosGenerationStateChanged(
    bool is_generating) {
  page_->OnFirstPartyAutoTodosGenerationStateChanged(is_generating);
}

void ContextHubPageHandler::OnThirdPartyAutoTodosGenerationStateChanged(
    bool is_generating) {
  page_->OnThirdPartyAutoTodosGenerationStateChanged(is_generating);
}

void ContextHubPageHandler::OnMemoryBankEntryAdded(
    const context_hub::MemoryBankEntry& entry) {
  page_->OnMemoryBankEntryAdded(entry);
}

void ContextHubPageHandler::OnMemoryBankEntryUpdated(
    int64_t id,
    const std::vector<std::string>& tags,
    const std::optional<std::string>& note,
    const std::optional<std::string>& collection) {
  auto annotations =
      browser::context_hub::mojom::MemoryBankEntryAnnotations::New();
  annotations->collection = collection;
  annotations->note = note;
  annotations->tags = tags;
  page_->OnMemoryBankEntryUpdated(id, std::move(annotations));
}

void ContextHubPageHandler::OnMemoryBankEntriesDeleted(
    const std::vector<int64_t>& ids) {
  page_->OnMemoryBankEntriesDeleted(ids);
}

void ContextHubPageHandler::GenerateFirstPartyAutoTodos(
    GenerateFirstPartyAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(false);
    return;
  }

  service->GenerateFirstPartyAutoTodos(std::move(callback));
}

void ContextHubPageHandler::GetAutoTodos(GetAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({}, {}, context_hub::AutoTodosGenerationMetadata(),
                            context_hub::AutoTodosGenerationMetadata());
    return;
  }

  context_hub::AutoTodosGenerationMetadata first_party_metadata =
      service->GetFirstPartyGenerationMetadata();
  context_hub::AutoTodosGenerationMetadata third_party_metadata =
      service->GetThirdPartyGenerationMetadata();

  service->GetAutoTodos(base::BindOnce(
      [](GetAutoTodosCallback callback,
         context_hub::AutoTodosGenerationMetadata first_party_metadata,
         context_hub::AutoTodosGenerationMetadata third_party_metadata,
         std::vector<context_hub::AutoTodoEntry> entries) {
        std::vector<context_hub::AutoTodoEntry> first_party_todos;
        std::vector<context_hub::AutoTodoEntry> third_party_todos;
        for (context_hub::AutoTodoEntry& entry : entries) {
          if (entry.is_first_party()) {
            first_party_todos.push_back(std::move(entry));
          } else if (entry.is_third_party()) {
            third_party_todos.push_back(std::move(entry));
          }
        }
        std::move(callback).Run(std::move(first_party_todos),
                                std::move(third_party_todos),
                                first_party_metadata, third_party_metadata);
      },
      std::move(callback), first_party_metadata, third_party_metadata));
}

void ContextHubPageHandler::UpdateAutoTodo(
    const context_hub::AutoTodoEntry& todo,
    UpdateAutoTodoCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run(false);
    return;
  }

  service->UpdateAutoTodo(todo, std::move(callback));
}

void ContextHubPageHandler::ClearFirstPartyAutoTodos(
    ClearFirstPartyAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearFirstPartyAutoTodos(std::move(callback));
    return;
  }
  std::move(callback).Run(false);
}

void ContextHubPageHandler::ClearThirdPartyAutoTodos(
    ClearThirdPartyAutoTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearThirdPartyAutoTodos(std::move(callback));
    return;
  }
  std::move(callback).Run(false);
}

void ContextHubPageHandler::SetTodoFeedback(
    browser::context_hub::mojom::AutoTodoItemFeedbackPtr feedback,
    SetTodoFeedbackCallback callback) {
  CHECK(feedback);
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->SetTodoFeedback(std::move(feedback));
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::DeleteTodoFeedback(
    const std::string& id,
    DeleteTodoFeedbackCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->DeleteTodoFeedback(id);
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::ClearTodoFeedbacks(
    ClearTodoFeedbacksCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearTodoFeedbacks();
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::GetTodoFeedbacks(
    GetTodoFeedbacksCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    std::move(callback).Run(service->GetTodoFeedbacks());
    return;
  }
  std::move(callback).Run({});
}

void ContextHubPageHandler::GetSaveToMemoryBankContext(
    GetSaveToMemoryBankContextCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    if (auto pending = service->GetPendingMemoryBankEntry()) {
      auto mojo_context =
          browser::context_hub::mojom::SaveToMemoryBankContext::New();
      mojo_context->url = pending->url;
      mojo_context->tab_title = pending->tab_title;
      if (pending->type == context_hub::MemoryBankType::kTextSelection &&
          pending->selected_text.has_value()) {
        // Truncate the snippet to a reasonable length since we only need a
        // preview in the UI.
        static constexpr size_t kMaxSnippetPreviewLength = 300;
        std::string preview = *pending->selected_text;
        if (preview.length() > kMaxSnippetPreviewLength) {
          preview.resize(kMaxSnippetPreviewLength);
        }
        mojo_context->selected_text = std::move(preview);
      }
      mojo_context->image_url = pending->image_url;
      std::move(callback).Run(std::move(mojo_context));
      return;
    }
  }
  std::move(callback).Run(nullptr);
}

void ContextHubPageHandler::GetAllMemoryBankEntries(
    GetAllMemoryBankEntriesCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->GetAllEntries(base::BindOnce(
      [](GetAllMemoryBankEntriesCallback callback,
         std::vector<context_hub::MemoryBankEntry> entries) {
        std::move(callback).Run(entries);
      },
      std::move(callback)));
}

void ContextHubPageHandler::DeleteMemoryBankEntries(
    const std::vector<int64_t>& ids,
    DeleteMemoryBankEntriesCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run();
    return;
  }

  service->DeleteEntries(ids, base::IgnoreArgs<bool>(std::move(callback)));
}

void ContextHubPageHandler::SaveMemoryBankEntry(
    browser::context_hub::mojom::MemoryBankEntryAnnotationsPtr annotations,
    SaveMemoryBankEntryCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (service && annotations) {
    std::vector<std::string> tags =
        std::move(annotations->tags).value_or(std::vector<std::string>{});
    bool success = service->SavePendingMemoryBankEntry(
        std::move(tags), std::move(annotations->note),
        std::move(annotations->collection));
    std::move(callback).Run(success);
    return;
  }
  std::move(callback).Run(/*success=*/false);
}

void ContextHubPageHandler::GetAllMemoryBankTags(
    GetAllMemoryBankTagsCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->GetAllMemoryBankTags(std::move(callback));
}

void ContextHubPageHandler::GetAllMemoryBankCollections(
    GetAllMemoryBankCollectionsCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->GetAllMemoryBankCollections(std::move(callback));
}

void ContextHubPageHandler::UpdateMemoryBankEntryAnnotations(
    int64_t id,
    browser::context_hub::mojom::MemoryBankEntryAnnotationsPtr annotations,
    UpdateMemoryBankEntryAnnotationsCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !annotations) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::vector<std::string> tags =
      std::move(annotations->tags).value_or(std::vector<std::string>{});
  service->UpdateMemoryBankEntryAnnotations(
      id, std::move(tags), std::move(annotations->note),
      std::move(annotations->collection), std::move(callback));
}

void ContextHubPageHandler::UploadMemoryBankEntries(
    const std::vector<context_hub::MemoryBankEntry>& entries,
    UploadMemoryBankEntriesCallback callback) {
  auto* service = ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || entries.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  std::vector<context_hub::MemoryBankEntry> valid_entries;
  valid_entries.reserve(entries.size());
  for (const auto& entry : entries) {
    if (!entry.url.is_valid() || !entry.url.SchemeIsHTTPOrHTTPS()) {
      continue;
    }

    context_hub::MemoryBankEntry valid_entry = entry;
    // Reset `id` to 0 so the storage backend treats the uploaded entry as a
    // new entry (generating a fresh ID and enforcing capacity limits) rather
    // than overwriting an existing entry.
    valid_entry.id = 0;
    valid_entries.push_back(std::move(valid_entry));
  }

  if (valid_entries.empty()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  // TODO(crbug.com/567261012): Surface partial or complete upload failures to
  // the user when any of the uploaded entries fail to save or are invalid.
  auto barrier_callback = base::BarrierCallback<bool>(
      valid_entries.size(), base::BindOnce(
                                [](UploadMemoryBankEntriesCallback callback,
                                   const std::vector<bool>& results) {
                                  bool any_succeeded = std::ranges::any_of(
                                      results, std::identity{});
                                  std::move(callback).Run(any_succeeded);
                                },
                                std::move(callback)));

  for (auto& entry : base::Reversed(valid_entries)) {
    service->SaveMemoryBankEntry(std::move(entry), barrier_callback);
  }
}

namespace {

std::vector<context_hub::TabData> GetOpenUngroupedTabs(
    ContextHubPageHandler::TabProvider* tab_provider) {
  std::vector<context_hub::TabData> tabs;
#if !BUILDFLAG(IS_ANDROID)
  if (tab_provider) {
    for (content::WebContents* tab_contents :
         tab_provider->GetUngroupedTabs()) {
      SessionID session_id = sessions::SessionTabHelper::IdForTab(tab_contents);
      if (session_id.is_valid()) {
        tabs.push_back({session_id.id(),
                        base::UTF16ToUTF8(tab_contents->GetTitle()),
                        tab_contents->GetLastCommittedURL()});
      }
    }
  }
#endif
  return tabs;
}

std::vector<browser::context_hub::mojom::TabInfoPtr> ToMojoTabs(
    const std::vector<context_hub::TabData>& tabs) {
  std::vector<browser::context_hub::mojom::TabInfoPtr> mojo_tabs;
  mojo_tabs.reserve(tabs.size());
  for (const auto& tab : tabs) {
    auto mojo_tab = browser::context_hub::mojom::TabInfo::New();
    mojo_tab->id = tab.id;
    mojo_tab->title = tab.title;
    mojo_tab->url = tab.url;
    mojo_tabs.push_back(std::move(mojo_tab));
  }
  return mojo_tabs;
}

std::vector<browser::context_hub::mojom::ChatMessagePtr> ToMojoChatHistory(
    const std::vector<optimization_guide::proto::ChatHistoryTurn>& history) {
  std::vector<browser::context_hub::mojom::ChatMessagePtr> mojo_history;
  mojo_history.reserve(history.size());
  for (const auto& turn : history) {
    auto mojo_msg = browser::context_hub::mojom::ChatMessage::New();
    mojo_msg->role =
        turn.role() == optimization_guide::proto::ChatHistoryTurn::ROLE_USER
            ? browser::context_hub::mojom::ChatRole::kUser
            : browser::context_hub::mojom::ChatRole::kAssistant;
    mojo_msg->content = turn.message_content();
    mojo_history.push_back(std::move(mojo_msg));
  }
  return mojo_history;
}

#if !BUILDFLAG(IS_ANDROID)
// Returns the Glic service for `profile`, or null if Glic is not available for
// it. Keeps the definition of "Glic is available" in one place.
glic::GlicKeyedService* GetGlicServiceIfEnabled(Profile* profile) {
  if (!glic::GlicEnabling::IsEnabledForProfile(profile)) {
    return nullptr;
  }
  return glic::GlicKeyedService::Get(profile);
}
#endif

}  // namespace

void ContextHubPageHandler::GenerateTabBasedTodos(
    GenerateTabBasedTodosCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run(false);
    return;
  }

  std::vector<base::WeakPtr<content::WebContents>> tab_contents;
  for (content::WebContents* wc : tab_provider_->GetTabs()) {
    if (wc) {
      tab_contents.push_back(wc->GetWeakPtr());
    }
  }

  service->GenerateTabBasedTodos(std::move(tab_contents), std::move(callback));
}

void ContextHubPageHandler::GetTabs(GetTabsCallback callback) {
  std::move(callback).Run(
      ToMojoTabs(GetOpenUngroupedTabs(tab_provider_.get())));
}

void ContextHubPageHandler::RetrieveAndGroupTabs(
    const std::string& user_command,
    RetrieveAndGroupTabsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run({}, {}, /*llm_response=*/nullptr);
    return;
  }

  service->GroupTabs(
      GetOpenUngroupedTabs(tab_provider_.get()), user_command,
      base::BindOnce(
          [](RetrieveAndGroupTabsCallback callback,
             std::vector<context_hub::TabGroupEntry> groups,
             std::vector<context_hub::TabData> ungrouped_tabs,
             std::string text_response) {
            std::vector<browser::context_hub::mojom::TabGroupPtr> mojo_groups;
            for (const auto& group : groups) {
              auto mojo_group = browser::context_hub::mojom::TabGroup::New();
              mojo_group->label = group.label;
              mojo_group->tabs = ToMojoTabs(group.tabs);
              mojo_groups.push_back(std::move(mojo_group));
            }

            browser::context_hub::mojom::ChatMessagePtr mojo_llm_response;
            if (!text_response.empty()) {
              mojo_llm_response =
                  browser::context_hub::mojom::ChatMessage::New();
              mojo_llm_response->role =
                  browser::context_hub::mojom::ChatRole::kAssistant;
              mojo_llm_response->content = std::move(text_response);
            }

            std::move(callback).Run(std::move(mojo_groups),
                                    ToMojoTabs(ungrouped_tabs),
                                    std::move(mojo_llm_response));
          },
          std::move(callback)));
}

void ContextHubPageHandler::GetExistingTabGroupsAndChats(
    GetExistingTabGroupsAndChatsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run({}, {}, {});
    return;
  }

  std::vector<context_hub::TabData> open_tabs =
      GetOpenUngroupedTabs(tab_provider_.get());
  std::vector<browser::context_hub::mojom::ChatMessagePtr> mojo_history =
      ToMojoChatHistory(service->GetTabGroupChatHistory());

  if (open_tabs.empty()) {
    std::move(callback).Run({}, {}, std::move(mojo_history));
    return;
  }

  service->GetTabGroups(base::BindOnce(
      [](std::vector<context_hub::TabData> open_tabs,
         std::vector<browser::context_hub::mojom::ChatMessagePtr> mojo_history,
         GetExistingTabGroupsAndChatsCallback callback,
         std::vector<context_hub::TabGroupEntry> stored_groups) {
        base::flat_map<int32_t, size_t> tab_index_map;
        for (size_t i = 0; i < open_tabs.size(); ++i) {
          // Populate the map with tab ID to index.
          tab_index_map.emplace(open_tabs[i].id, i);
        }

        std::vector<browser::context_hub::mojom::TabGroupPtr> mojo_groups;
        // For each stored group, go through each tab in the group and find
        // the corresponding tab by ID in the open tabs list. Delete the tab ID
        // from the map once added to a group.
        for (const auto& entry : stored_groups) {
          std::vector<context_hub::TabData> group_tabs;
          for (int64_t tab_id_64 : entry.tab_ids) {
            int32_t tab_id = static_cast<int32_t>(tab_id_64);
            auto it = tab_index_map.find(tab_id);
            if (it != tab_index_map.end()) {
              group_tabs.push_back(open_tabs[it->second]);
              tab_index_map.erase(it);
            }
          }
          auto mojo_group = browser::context_hub::mojom::TabGroup::New();
          mojo_group->label = entry.label;
          mojo_group->tabs = ToMojoTabs(group_tabs);
          mojo_groups.push_back(std::move(mojo_group));
        }

        // Any remaining tab IDs in the map are ungrouped tabs.
        std::vector<context_hub::TabData> ungrouped_tabs;
        for (const auto& tab : open_tabs) {
          if (tab_index_map.contains(tab.id)) {
            ungrouped_tabs.push_back(tab);
          }
        }

        std::move(callback).Run(std::move(mojo_groups),
                                ToMojoTabs(ungrouped_tabs),
                                std::move(mojo_history));
      },
      std::move(open_tabs), std::move(mojo_history), std::move(callback)));
}

void ContextHubPageHandler::SwitchToTab(int64_t tab_id) {
  if (tab_provider_) {
    tab_provider_->SwitchToTab(tab_id);
  }
}

void ContextHubPageHandler::CloseTab(int64_t tab_id) {
  if (tab_provider_) {
    tab_provider_->CloseTab(tab_id);
    context_hub::ContextHubService* service =
        ContextHubServiceFactory::GetForProfile(profile_);
    if (service) {
      service->DeleteAutoTodoByTabId(tab_id, base::DoNothing());
    }
  }
}

void ContextHubPageHandler::ClearTabGroups(ClearTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run();
    return;
  }

  service->DeleteAllTabGroups(std::move(callback));
}

void ContextHubPageHandler::ReplaceTabGroups(
    std::vector<browser::context_hub::mojom::TabGroupPtr> groups,
    ReplaceTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run();
    return;
  }

  std::vector<context_hub::TabGroupEntry> entries;
  entries.reserve(groups.size());
  for (auto& group : groups) {
    if (group->tabs.empty()) {
      continue;
    }
    context_hub::TabGroupEntry entry;
    entry.label = std::move(group->label);
    for (auto& tab : group->tabs) {
      entry.tab_ids.push_back(tab->id);
      entry.tabs.push_back({.id = tab->id,
                            .title = std::move(tab->title),
                            .url = std::move(tab->url)});
    }
    entries.push_back(std::move(entry));
  }
  service->ReplaceTabGroups(std::move(entries), std::move(callback));
}

void ContextHubPageHandler::ClearTabGroupChatHistory(
    ClearTabGroupChatHistoryCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearTabGroupChatHistory();
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::AskGeminiWithContext(
    const std::string& user_command,
    const std::vector<int64_t>& memory_bank_entry_ids,
    bool save_to_history,
    AskGeminiWithContextCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    context_hub::MemoryBankChatHistoryTurn response;
    response.turn.set_role(
        optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT);
    response.turn.set_message_content("Service unavailable.");
    std::move(callback).Run(std::move(response));
    return;
  }

  service->ExecuteMemoryBankChat(
      memory_bank_entry_ids, user_command, save_to_history,
      base::BindOnce(
          [](AskGeminiWithContextCallback callback,
             std::optional<std::string> response_text,
             std::vector<context_hub::MemoryBankChatCitation> citations) {
            context_hub::MemoryBankChatHistoryTurn response;
            response.turn.set_role(
                optimization_guide::proto::ChatHistoryTurn::ROLE_ASSISTANT);
            response.turn.set_message_content(
                std::move(response_text)
                    .value_or("Failed to generate response."));
            response.citations = std::move(citations);
            std::move(callback).Run(std::move(response));
          },
          std::move(callback)));
}

void ContextHubPageHandler::GetMemoryBankChatHistory(
    GetMemoryBankChatHistoryCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(service->GetMemoryBankChatHistory());
}

void ContextHubPageHandler::ClearMemoryBankChatHistory(
    ClearMemoryBankChatHistoryCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearMemoryBankChatHistory();
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::ConfirmAllTabGroups(
    ConfirmAllTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run(false);
    return;
  }

  service->GetTabGroups(base::BindOnce(
      [](base::WeakPtr<ContextHubPageHandler> handler,
         base::WeakPtr<context_hub::ContextHubService> service,
         ConfirmAllTabGroupsCallback callback,
         std::vector<context_hub::TabGroupEntry> groups) {
        if (!handler || !service) {
          std::move(callback).Run(false);
          return;
        }
        bool success = handler->tab_provider_->ConfirmTabGroups(groups);
        service->DeleteAllTabGroups(
            base::BindOnce(std::move(callback), success));
      },
      weak_factory_.GetWeakPtr(), service->GetWeakPtr(), std::move(callback)));
}

void ContextHubPageHandler::GetConfirmedTabGroups(
    GetConfirmedTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  std::vector<context_hub::TabGroupEntry> entries =
      service->GetConfirmedTabGroups();
  std::vector<browser::context_hub::mojom::TabGroupPtr> groups;
  groups.reserve(entries.size());
  for (const auto& entry : entries) {
    auto mojo_group = browser::context_hub::mojom::TabGroup::New();
    base::Uuid parsed_guid = base::Uuid::ParseCaseInsensitive(entry.id);
    if (parsed_guid.is_valid()) {
      mojo_group->saved_guid = parsed_guid;
    }
    mojo_group->label = entry.label;
    mojo_group->tabs = ToMojoTabs(entry.tabs);
    groups.push_back(std::move(mojo_group));
  }
  std::move(callback).Run(std::move(groups));
}

void ContextHubPageHandler::RemoveConfirmedTabGroup(
    const base::Uuid& saved_guid,
    RemoveConfirmedTabGroupCallback callback) {
  if (!saved_guid.is_valid()) {
    std::move(callback).Run();
    return;
  }

  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run();
    return;
  }

  tab_provider_->UngroupGroupFromTabstripIfOpen(saved_guid);
  service->RemoveConfirmedTabGroup(saved_guid);
  std::move(callback).Run();
}

void ContextHubPageHandler::CloseConfirmedTabGroup(
    const base::Uuid& saved_guid,
    CloseConfirmedTabGroupCallback callback) {
  if (!saved_guid.is_valid()) {
    std::move(callback).Run();
    return;
  }

  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run();
    return;
  }

  tab_provider_->RemoveGroupFromTabstripIfOpen(saved_guid);
  std::move(callback).Run();
}

void ContextHubPageHandler::RemoveAllConfirmedTabGroups(
    RemoveAllConfirmedTabGroupsCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service || !tab_provider_) {
    std::move(callback).Run();
    return;
  }

  for (const context_hub::TabGroupEntry& entry :
       service->GetConfirmedTabGroups()) {
    base::Uuid guid = base::Uuid::ParseCaseInsensitive(entry.id);
    if (guid.is_valid()) {
      tab_provider_->UngroupGroupFromTabstripIfOpen(guid);
    }
  }

  service->RemoveAllConfirmedTabGroups();
  std::move(callback).Run();
}

void ContextHubPageHandler::ExecuteSmartSearch(
    const std::string& query,
    ExecuteSmartSearchCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (!service) {
    std::move(callback).Run({});
    return;
  }

  service->ExecuteSmartSearch(query, std::move(callback));
}

void ContextHubPageHandler::OpenUrlsInTabGroup(
    const std::string& group_label,
    const std::vector<GURL>& urls,
    OpenUrlsInTabGroupCallback callback) {
  if (!base::FeatureList::IsEnabled(browser::context_hub::mojom::kAutoTodos) &&
      !base::FeatureList::IsEnabled(browser::context_hub::mojom::kTopics) &&
      !base::FeatureList::IsEnabled(
          browser::context_hub::mojom::kSmartSearch)) {
    receiver_.ReportBadMessage(
        "OpenUrlsInTabGroup cannot be called without AutoTodos, Topics, or "
        "SmartSearch enabled.");
    return;
  }

  if (!tab_provider_) {
    std::move(callback).Run(false);
    return;
  }

  // Only open web pages. These URLs come from the renderer, and ultimately
  // from browsing history, so they can carry schemes such as file:, chrome:
  // or javascript: that must not be opened without an explicit user
  // navigation. They are dropped rather than reported as a bad message, since
  // a history-derived list can legitimately contain them. Filtering happens
  // before capping so that rejected URLs don't count towards the limit.
  constexpr size_t kMaxUrlsToOpen = 10;
  std::vector<GURL> web_urls;
  for (const GURL& url : urls) {
    if (web_urls.size() == kMaxUrlsToOpen) {
      break;
    }
    if (url.is_valid() && url.SchemeIsHTTPOrHTTPS()) {
      web_urls.push_back(url);
    }
  }
  if (web_urls.empty()) {
    std::move(callback).Run(false);
    return;
  }

  bool success = tab_provider_->OpenUrlsInTabGroup(group_label, web_urls);
  std::move(callback).Run(success);
}

namespace {

// Converts a journey already resolved by the history backend into its Mojo
// representation. No further lookups are needed: the history service hands
// back visits that carry their URL and title, and drops any journey it could
// not fully resolve.
browser::context_hub::mojom::TopicPtr ToMojoTopic(Journey journey) {
  std::vector<browser::context_hub::mojom::TopicVisitPtr> visits =
      base::ToVector(journey.visits, [](JourneyVisit& visit) {
        return browser::context_hub::mojom::TopicVisit::New(
            std::move(visit.url), base::UTF16ToUTF8(visit.title),
            visit.visit_time);
      });

  std::vector<browser::context_hub::mojom::TopicContinuationQueryPtr>
      continuation_queries = base::ToVector(
          journey.continuation_queries, [](JourneyContinuationQuery& query) {
            return browser::context_hub::mojom::TopicContinuationQuery::New(
                std::move(query.title), std::move(query.prompt));
          });

  std::vector<browser::context_hub::mojom::TopicCollectionPtr> collections =
      base::ToVector(
          journey.collections, [](JourneyVisitCollection& collection) {
            return browser::context_hub::mojom::TopicCollection::New(
                std::move(collection.title),
                base::ToVector(collection.visits, [](JourneyVisit& visit) {
                  return browser::context_hub::mojom::TopicCollectionItem::New(
                      base::UTF16ToUTF8(visit.title), std::move(visit.url),
                      /*site_name=*/std::nullopt, visit.visit_time);
                }));
          });

  return browser::context_hub::mojom::Topic::New(
      /*id=*/std::move(journey.journey_id), std::move(journey.title),
      journey.creation_time, std::move(journey.emoji),
      std::move(journey.overview), std::move(journey.short_overview),
      std::move(visits), std::move(continuation_queries),
      std::move(collections));
}

std::vector<browser::context_hub::mojom::TopicPtr> ToMojoTopics(
    std::vector<Journey> journeys) {
  return base::ToVector(journeys, [](Journey& journey) {
    return ToMojoTopic(std::move(journey));
  });
}

}  // namespace

void ContextHubPageHandler::GetTopics(GetTopicsCallback callback) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service) {
    std::move(callback).Run({});
    return;
  }

  // The reply comes back on this sequence. `topics_task_tracker_` is owned by
  // this handler, so a query outliving it is cancelled instead of replying.
  history_service->GetAllJourneys(
      base::BindOnce(
          [](GetTopicsCallback callback, std::vector<Journey> journeys) {
            std::move(callback).Run(ToMojoTopics(std::move(journeys)));
          },
          std::move(callback)),
      &topics_task_tracker_);
}

void ContextHubPageHandler::GetTopic(const std::string& id,
                                     GetTopicCallback callback) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service || id.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  // See `GetTopics()` for why the reply is safe to run on this handler.
  history_service->GetJourney(
      id,
      base::BindOnce(
          [](GetTopicCallback callback, std::optional<Journey> journey) {
            std::move(callback).Run(
                journey ? ToMojoTopic(std::move(journey).value()) : nullptr);
          },
          std::move(callback)),
      &topics_task_tracker_);
}

void ContextHubPageHandler::GetTopicPageImageUrl(
    const GURL& page_url,
    GetTopicPageImageUrlCallback callback) {
  page_image_service::ImageService* image_service =
      page_image_service::ImageServiceFactory::GetForBrowserContext(profile_);
  if (!image_service || !page_url.is_valid() ||
      !page_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // The reply does not touch this handler, and the service always replies, so
  // there is nothing to cancel if the handler goes away first.
  image_service->FetchImageFor(
      page_image_service::mojom::ClientId::ContextHubTopics, page_url,
      page_image_service::mojom::Options(),
      base::BindOnce(
          [](GetTopicPageImageUrlCallback callback, const GURL& image_url) {
            std::move(callback).Run(image_url.is_valid()
                                        ? std::make_optional(image_url)
                                        : std::nullopt);
          },
          std::move(callback)));
}

namespace {

// Size, in pixels, of the favicon whose dominant color is computed. Matches
// the size of the favicon drawn in a topic collection card.
constexpr int kTopicPageFaviconColorSourceSize = 32;

// Returns the dominant color of the PNG-encoded favicon `png_bytes`, with its
// lightness clamped like the background of fallback icons. Decodes the image,
// so it runs off the UI thread. Returns an optional to match the reply of
// `GetTopicPageFaviconColor()`.
std::optional<SkColor> ComputeFaviconDominantColor(
    scoped_refptr<base::RefCountedMemory> png_bytes) {
  favicon_base::FallbackIconStyle style;
  favicon_base::SetDominantColorAsBackground(*png_bytes, &style);
  return style.background_color;
}

}  // namespace

void ContextHubPageHandler::GetTopicPageFaviconColor(
    const GURL& page_url,
    GetTopicPageFaviconColorCallback callback) {
  favicon::FaviconService* favicon_service =
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!favicon_service || !page_url.is_valid() ||
      !page_url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // Looks the favicon up from local storage only, falling back to the host's
  // favicon, the same way chrome://favicon2 does for the card's favicon.
  favicon_service->GetRawFaviconForPageURL(
      page_url, {favicon_base::IconType::kFavicon},
      kTopicPageFaviconColorSourceSize, /*fallback_to_host=*/true,
      base::BindOnce(&ContextHubPageHandler::OnTopicPageFaviconLoaded,
                     weak_factory_.GetWeakPtr(), std::move(callback)),
      &favicon_task_tracker_);
}

void ContextHubPageHandler::OnTopicPageFaviconLoaded(
    GetTopicPageFaviconColorCallback callback,
    const favicon_base::FaviconRawBitmapResult& result) {
  if (!result.is_valid()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  // The reply only runs `callback`, so there is nothing to cancel if the
  // handler goes away first.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE,
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&ComputeFaviconDominantColor, result.bitmap_data),
      std::move(callback));
}

void ContextHubPageHandler::OpenTopic(
    browser::context_hub::mojom::TopicIdOrUrlPtr topic_id_or_url) {
  if (tab_provider_) {
    tab_provider_->OpenTopic(std::move(topic_id_or_url));
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  // Fallback for when there is no tab provider: open the topic from the
  // hosting WebContents instead.
  GURL url = TabProvider::ResolveTopicUrl(topic_id_or_url);
  if (!url.is_valid()) {
    return;
  }

  if (web_contents_) {
    content::OpenURLParams params(url, content::Referrer(),
                                  WindowOpenDisposition::kNewForegroundTab,
                                  ui::PAGE_TRANSITION_LINK,
                                  /*is_renderer_initiated=*/false);
    web_contents_->OpenURL(params, /*navigation_handle_callback=*/{});
  }
#endif
}

void ContextHubPageHandler::OpenGlicPanel(
    const std::vector<std::string>& prompts) {
#if !BUILDFLAG(IS_ANDROID)
  glic::GlicKeyedService* glic_service = GetGlicServiceIfEnabled(profile_);
  if (!glic_service || !web_contents_) {
    return;
  }
  // The WebUI may be hosted outside of a tab (e.g. in a dialog), in which case
  // there is no tab to bind the side panel to.
  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents_);
  if (!tab) {
    return;
  }

  // Closing the panel keeps the instance alive (preserving the conversation)
  // but stops it from showing, so this distinguishes "visible right now" from
  // "closed but remembered".
  glic::GlicInstance* instance = glic_service->GetInstanceForTab(tab);
  const bool already_showing = instance && instance->IsShowing();

  glic::GlicInvokeOptions options(
      glic::Target(*tab), glic::mojom::InvocationSource::kContextHubTopics);
  // The topic page offers up to three suggested prompts; clamp defensively
  // since the list comes from the renderer.
  constexpr size_t kMaxPrompts = 3;
  base::span<const std::string> capped_prompts =
      base::span(prompts).first(std::min(prompts.size(), kMaxPrompts));
  options.prompts.assign(capped_prompts.begin(), capped_prompts.end());
  // With two or more topic-specific prompts the page has enough of its own
  // suggestions, so ask the web client to suppress the generic Zero State
  // Suggestions in favor of them. With fewer than two (e.g. a topic with no
  // title, or a single prompt), keep ZSS so the panel still has a useful set
  // of suggestions to offer.
  //
  // TODO(crbug.com/567878517): The web client doesn't support this yet. It
  // ignores `disable_zss`, and with more than one prompt it shows the generic
  // ZSS rather than these prompts.
  constexpr size_t kMinPromptsToDisableZss = 2;
  options.disable_zss = options.prompts.size() >= kMinPromptsToDisableZss;

  if (already_showing) {
    // The invoke is still delivered to the web client, so it can replace its
    // suggestions with this topic's once supported (see TODO above). Since the
    // user is already looking at the panel, refreshing it should not steal
    // focus from the topic page, and should not fail if another invocation
    // happens to be in flight.
    options.focus_on_show = false;
    options.supersede_if_in_progress = true;
  } else {
    // The invoke is sent on page load, while the side panel is still sliding
    // open. Delivering (and focusing) it before the panel and the tab have
    // settled at their final size leaves the web client laid out for the
    // in-progress size, which shows up as shifted content once the panel is
    // re-laid out (e.g. after a navigation or tab switch).
    options.wait_for_panel_open = true;
  }

  // The FRE is rendered inside the panel, so the panel opens either way. But
  // until the user consents, delivery of `prompts` is blocked on FRE
  // completion, and the invocation's default watchdog is only one minute;
  // far too short to read and accept a consent screen. Without a longer
  // timeout the invocation is abandoned and `prompts` are never delivered,
  // even though the user did eventually consent.
  //
  // TODO(crbug.com/564810188): This only narrows the window. If the user
  // abandons the FRE the invocation still fails silently, because the page has
  // no error surface and no manual affordance to open Glic.
  if (!glic::GlicEnabling::HasConsentedForProfile(profile_)) {
    options.timeout = base::Minutes(5);
  }

  glic_service->Invoke(std::move(options));
#endif
}

void ContextHubPageHandler::SetTopicFeedback(
    browser::context_hub::mojom::TopicFeedbackPtr feedback,
    SetTopicFeedbackCallback callback) {
  if (feedback->id.empty()) {
    receiver_.ReportBadMessage("SetTopicFeedback requires a topic id.");
    return;
  }
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->SetTopicFeedback(std::move(feedback));
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::DeleteTopicFeedback(
    const std::string& topic_id,
    DeleteTopicFeedbackCallback callback) {
  if (topic_id.empty()) {
    receiver_.ReportBadMessage("DeleteTopicFeedback requires a topic id.");
    return;
  }
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->DeleteTopicFeedback(topic_id);
  }
  std::move(callback).Run();
}

void ContextHubPageHandler::GetTopicFeedbacks(
    GetTopicFeedbacksCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    std::move(callback).Run(service->GetTopicFeedbacks());
    return;
  }
  std::move(callback).Run({});
}

void ContextHubPageHandler::ClearTopicFeedbacks(
    ClearTopicFeedbacksCallback callback) {
  context_hub::ContextHubService* service =
      ContextHubServiceFactory::GetForProfile(profile_);
  if (service) {
    service->ClearTopicFeedbacks();
  }
  std::move(callback).Run();
}

namespace {

// Returns the Chrome and Topics experiment state recorded in the export.
context_hub::TopicsFeedbackExportContext BuildTopicsFeedbackExportContext(
    base::Time now,
    base::Time window_start,
    int window_days) {
  context_hub::TopicsFeedbackExportContext context;
  context.exported_at = now;
  context.window_start = window_start;
  context.window_days = window_days;
  context.chrome_version = std::string(version_info::GetVersionNumber());
  context.chrome_channel =
      std::string(version_info::GetChannelString(chrome::GetChannel()));
  if (base::FieldTrial* trial = base::FeatureList::GetFieldTrial(
          browser::context_hub::mojom::kTopics)) {
    context.finch = context_hub::TopicsFeedbackExportContext::FinchGroup{
        .trial = trial->trial_name(), .group = trial->group_name()};
  }
  base::GetFieldTrialParamsByFeature(browser::context_hub::mojom::kTopics,
                                     &context.feature_params);
  return context;
}

}  // namespace

void ContextHubPageHandler::GetTopicsFeedbackExportPreview(
    int32_t window_days,
    GetTopicsFeedbackExportPreviewCallback callback) {
  if (!context_hub::features::kTopicsFishfoodFeedback.Get()) {
    receiver_.ReportBadMessage(
        "GetTopicsFeedbackExportPreview requires fishfood feedback.");
    return;
  }

  FetchTopicsFeedbackExportData(
      base::Time::Now() -
          base::Days(context_hub::ClampTopicsFeedbackWindowDays(window_days)),
      base::BindOnce(
          [](GetTopicsFeedbackExportPreviewCallback callback,
             context_hub::TopicsFeedbackExportData data) {
            std::move(callback).Run(
                context_hub::BuildTopicsFeedbackExportPreview(data));
          },
          std::move(callback)));
}

void ContextHubPageHandler::GenerateTopicsFeedbackBundle(
    browser::context_hub::mojom::TopicsFeedbackExportOptionsPtr options,
    GenerateTopicsFeedbackBundleCallback callback) {
  if (!context_hub::features::kTopicsFishfoodFeedback.Get()) {
    receiver_.ReportBadMessage(
        "GenerateTopicsFeedbackBundle requires fishfood feedback.");
    return;
  }

  const int window_days =
      context_hub::ClampTopicsFeedbackWindowDays(options->window_days);
  const base::Time now = base::Time::Now();
  const base::Time window_start = now - base::Days(window_days);
  FetchTopicsFeedbackExportData(
      window_start,
      base::BindOnce(
          [](GenerateTopicsFeedbackBundleCallback callback,
             const context_hub::TopicsFeedbackExportContext& context,
             browser::context_hub::mojom::TopicsFeedbackExportOptionsPtr
                 options,
             context_hub::TopicsFeedbackExportData data) {
            std::move(callback).Run(context_hub::BuildTopicsFeedbackBundle(
                data, context, *options));
          },
          std::move(callback),
          BuildTopicsFeedbackExportContext(now, window_start, window_days),
          std::move(options)));
}

void ContextHubPageHandler::FetchTopicsFeedbackExportData(
    base::Time window_start,
    base::OnceCallback<void(context_hub::TopicsFeedbackExportData)> callback) {
  auto data = std::make_unique<context_hub::TopicsFeedbackExportData>();
  data->window_start = window_start;
  if (context_hub::ContextHubService* service =
          ContextHubServiceFactory::GetForProfile(profile_)) {
    data->feedbacks = service->GetTopicFeedbacks();
  }

  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service) {
    std::move(callback).Run(std::move(*data));
    return;
  }

  // The three queries below fill in `data` and then run `barrier`, which owns
  // `data` and runs `callback` once all three have replied. If this handler
  // is destroyed first, `topics_task_tracker_` cancels the outstanding
  // replies, which destroys `barrier` and `data` without running `callback`.
  context_hub::TopicsFeedbackExportData* data_ptr = data.get();
  base::RepeatingClosure barrier = base::BarrierClosure(
      3, base::BindOnce(
             [](std::unique_ptr<context_hub::TopicsFeedbackExportData> data,
                base::OnceCallback<void(context_hub::TopicsFeedbackExportData)>
                    callback) { std::move(callback).Run(std::move(*data)); },
             std::move(data), std::move(callback)));

  history_service->GetAllJourneys(
      base::BindOnce(
          [](context_hub::TopicsFeedbackExportData* data,
             base::RepeatingClosure done, std::vector<Journey> journeys) {
            data->journeys = std::move(journeys);
            done.Run();
          },
          base::Unretained(data_ptr), barrier),
      &topics_task_tracker_);

  history_service->GetUnresolvableJourneysCountForFishfood(
      base::BindOnce(
          [](context_hub::TopicsFeedbackExportData* data,
             base::RepeatingClosure done, size_t count) {
            data->unresolvable_topics = count;
            done.Run();
          },
          base::Unretained(data_ptr), barrier),
      &topics_task_tracker_);

  history::QueryOptions options;
  options.begin_time = window_start;
  options.duplicate_policy = history::QueryOptions::KEEP_ALL_DUPLICATES;
  options.policy_for_404_visits = history::VisitQuery404sPolicy::kInclude404s;
  history_service->GetAnnotatedVisits(
      options, /*compute_redirect_chain_start_properties=*/false,
      /*get_unclustered_visits_only=*/false,
      base::BindOnce(
          [](context_hub::TopicsFeedbackExportData* data,
             base::RepeatingClosure done,
             std::vector<history::AnnotatedVisit> visits) {
            data->visits.reserve(visits.size());
            for (const history::AnnotatedVisit& visit : visits) {
              data->visits.push_back(context_hub::TopicsFeedbackHistoryVisit{
                  .url = visit.url_row.url(),
                  .title = visit.url_row.title(),
                  .visit_time = visit.visit_row.visit_time,
                  .is_foreign = !visit.visit_row.originator_cache_guid.empty(),
              });
            }
            done.Run();
          },
          base::Unretained(data_ptr), barrier),
      &topics_task_tracker_);
}

void ContextHubPageHandler::RunTodoTask(const std::string& prompt,
                                        RunTodoTaskCallback callback) {
#if !BUILDFLAG(IS_ANDROID)
  if (prompt.empty()) {
    std::move(callback).Run(false);
    return;
  }

  glic::GlicKeyedService* glic_service = GetGlicServiceIfEnabled(profile_);
  if (!glic_service || !web_contents_) {
    std::move(callback).Run(false);
    return;
  }

  tabs::TabInterface* tab =
      tabs::TabInterface::MaybeGetFromContents(web_contents_);
  if (!tab) {
    std::move(callback).Run(false);
    return;
  }

  glic::GlicInvokeOptions options(
      glic::mojom::InvocationSource::kContextHubAutoTodos);
  options.prompts.push_back(prompt);
  options.target.conversation = glic::NewConversation();
  options.feature_mode = glic::mojom::FeatureMode::kActuation;
  options.target.actuation_target = glic::mojom::ActuationTarget::kAgentDecides;
  options.target.surface =
      glic::DefaultSurface{tab->GetBrowserWindowInterface()};

  glic::GlicInvokeWithAutoSubmitOptions auto_submit_options;
  auto_submit_options.show_panel = true;

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options), std::move(auto_submit_options));
  std::move(callback).Run(true);
#else
  std::move(callback).Run(false);
#endif
}

void ContextHubPageHandler::OpenWebUrl(const GURL& url,
                                       OpenWebUrlCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#else
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    receiver_.ReportBadMessage(
        "Invalid or disallowed URL scheme in OpenWebUrl");
    return;
  }

  if (BrowserWindowInterface* browser =
          GetBrowserWindowInterface(web_contents_, profile_)) {
    chrome::AddSelectedTabWithURL(browser, url, ui::PAGE_TRANSITION_LINK);
  }
  std::move(callback).Run();
#endif
}

void ContextHubPageHandler::OpenContextHubPage(
    OpenContextHubPageCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#else
  if (BrowserWindowInterface* browser =
          GetBrowserWindowInterface(web_contents_, profile_)) {
    ShowSingletonTab(browser, GURL(chrome::kChromeUIContextHubURL));
  }
  std::move(callback).Run();
#endif
}

void ContextHubPageHandler::CopyAttachmentsToClipboard(
    std::vector<browser::context_hub::mojom::MemoryBankFileAttachmentPtr> files,
    CopyAttachmentsToClipboardCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#else
  if (files.empty() || files.size() > kMaxFilesToCopy) {
    receiver_.ReportBadMessage(
        "Invalid files count in CopyAttachmentsToClipboard");
    return;
  }

  // Reject oversized inputs with kInvalidInput so the client can retry with
  // smaller batches, rather than reporting a bad message.
  if (ExceedsAttachmentResourceLimits(files)) {
    std::move(callback).Run(
        browser::context_hub::mojom::CopyAttachmentsToClipboardStatus::
            kInvalidInput);
    return;
  }

  if (!HasValidUtf8Strings(files)) {
    receiver_.ReportBadMessage("Invalid UTF-8 in CopyAttachmentsToClipboard");
    return;
  }

  std::vector<std::pair<std::string, std::string>> sanitized_files =
      SanitizeAndDeduplicateAttachments(std::move(files));
  CHECK(!sanitized_files.empty());

  GetClipboardFileTaskRunner()->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&WriteAttachmentFilesToDisk, std::move(sanitized_files)),
      base::BindOnce(
          &ContextHubPageHandler::OnCopyAttachmentsToClipboardWritten,
          weak_factory_.GetWeakPtr(), std::move(callback)));
#endif  // !BUILDFLAG(IS_ANDROID)
}

#if !BUILDFLAG(IS_ANDROID)
void ContextHubPageHandler::OnCopyAttachmentsToClipboardWritten(
    CopyAttachmentsToClipboardCallback callback,
    std::vector<std::pair<std::string, base::FilePath>> written_files) {
  if (written_files.empty()) {
    std::move(callback).Run(
        browser::context_hub::mojom::CopyAttachmentsToClipboardStatus::
            kFileWriteFailed);
    return;
  }

  std::vector<ui::FileInfo> file_infos =
      base::ToVector(written_files, [](const auto& entry) {
        return ui::FileInfo(entry.second,
                            base::FilePath::FromUTF8Unsafe(entry.first));
      });

  ui::ScopedClipboardWriter scw(ui::ClipboardBuffer::kCopyPaste);
  scw.WriteFilenames(ui::FileInfosToURIList(file_infos));

  std::move(callback).Run(
      browser::context_hub::mojom::CopyAttachmentsToClipboardStatus::kSuccess);
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ContextHubPageHandler::StartSkillRecording() {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#else
  if (context_hub::SkillRecordingService* service =
          SkillRecordingServiceFactory::GetForProfile(profile_)) {
    service->StartRecording();
  }
#endif  // !BUILDFLAG(IS_ANDROID)
}

void ContextHubPageHandler::StopSkillRecording(
    StopSkillRecordingCallback callback) {
#if BUILDFLAG(IS_ANDROID)
  NOTREACHED();
#else
  if (context_hub::SkillRecordingService* service =
          SkillRecordingServiceFactory::GetForProfile(profile_)) {
    std::move(callback).Run(service->StopRecording());
    return;
  }
  std::move(callback).Run(
      browser::context_hub::mojom::SkillRecordingSessionData::New());
#endif  // !BUILDFLAG(IS_ANDROID)
}
