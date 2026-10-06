// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/generated_tool_definitions.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/tabs/public/tab_interface.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/url_util.h"

namespace {

std::optional<base::TimeDelta> ParseTimecode(const std::string& timecode) {
  std::vector<std::string> parts = base::SplitString(
      timecode, ":", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (parts.size() == 1) {
    int s;
    if (base::StringToInt(parts[0], &s)) {
      return base::Seconds(s);
    }
  } else if (parts.size() == 2) {
    int m, s;
    if (base::StringToInt(parts[0], &m) && base::StringToInt(parts[1], &s)) {
      return base::Seconds(m * 60 + s);
    }
  } else if (parts.size() == 3) {
    int h, m, s;
    if (base::StringToInt(parts[0], &h) && base::StringToInt(parts[1], &m) &&
        base::StringToInt(parts[2], &s)) {
      return base::Seconds(h * 3600 + m * 60 + s);
    }
  }
  return std::nullopt;
}

}  // namespace

namespace ttc {

AiOverlayTools::AiOverlayTools(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser,
    PageContextMonitor* page_context_monitor)
    : receiver_(this, std::move(receiver)),
      browser_(browser),
      page_context_monitor_(page_context_monitor) {}

AiOverlayTools::~AiOverlayTools() = default;

void AiOverlayTools::BindRegistryReceiver(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayToolRegistry>
        registry_receiver) {
  registry_receiver_.Bind(std::move(registry_receiver));
}

// static
void AiOverlayTools::RecordToolCallInvoked(std::string_view tool_name) {
  base::UmaHistogramBoolean(
      base::StrCat({"AI.OverlayDialog.ToolCallInvoked.", tool_name}), true);
}

// static
glic::InvokeWithAutoSubmitPasskey AiOverlayTools::GetGlicPassKey() {
  return glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey();
}

content::WebContents* AiOverlayTools::GetActiveWebContents() const {
  TabListInterface* tab_list = TabListInterface::From(browser());
  if (!tab_list) {
    return nullptr;
  }
  tabs::TabInterface* tab = tab_list->GetActiveTab();
  return tab ? tab->GetContents() : nullptr;
}

void AiOverlayTools::OpenUrl(const std::string& url_string,
                             bool new_tab,
                             OpenUrlCallback callback) {
  RecordToolCallInvoked("OpenUrl");
  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(base::unexpected("Invalid URL"));
    return;
  }

  if (!browser()) {
    std::move(callback).Run(base::unexpected("No browser window available"));
    return;
  }

  WindowOpenDisposition disposition =
      new_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
              : WindowOpenDisposition::CURRENT_TAB;
  // TODO(crbug.com/534887116): Report the navigation outcome instead of
  // succeeding immediately. On Android, OpenURL() may start the navigation
  // asynchronously.
  browser()->OpenURL(content::OpenURLParams::CreateBrowserInitiated(
                         url, disposition, ui::PAGE_TRANSITION_LINK),
                     /*navigation_handle_callback=*/{});
  std::move(callback).Run(std::monostate());
}

void AiOverlayTools::FollowLink(const std::string& id,
                                FollowLinkCallback callback) {
  RecordToolCallInvoked("FollowLink");
  CHECK(page_context_monitor_);

  std::string url = page_context_monitor_->GetUrlForHash(id);
  if (url.empty()) {
    std::move(callback).Run(
        base::unexpected(base::StrCat({"No link found for id ", id})));
    return;
  }

  OpenUrl(url, /*new_tab=*/false, std::move(callback));
}

void AiOverlayTools::PerformSearch(const std::string& query,
                                   bool new_tab,
                                   PerformSearchCallback callback) {
  RecordToolCallInvoked("PerformSearch");
  TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(browser_->GetProfile());
  if (!template_url_service) {
    std::move(callback).Run(base::unexpected("Search service not available"));
    return;
  }

  const TemplateURL* default_provider =
      template_url_service->GetDefaultSearchProvider();
  if (!default_provider) {
    std::move(callback).Run(
        base::unexpected("Default search provider not set"));
    return;
  }

  GURL url = default_provider->GenerateSearchURL(
      template_url_service->search_terms_data(), base::UTF8ToUTF16(query));
  OpenUrl(url.spec(), new_tab, std::move(callback));
}

void AiOverlayTools::GoBack(GoBackCallback callback) {
  RecordToolCallInvoked("GoBack");
  content::WebContents* contents = GetActiveWebContents();
  if (contents && contents->GetController().CanGoBack()) {
    contents->GetController().GoBack();
    std::move(callback).Run(std::monostate());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot go back"));
}

void AiOverlayTools::GoForward(GoForwardCallback callback) {
  RecordToolCallInvoked("GoForward");
  content::WebContents* contents = GetActiveWebContents();
  if (contents && contents->GetController().CanGoForward()) {
    contents->GetController().GoForward();
    std::move(callback).Run(std::monostate());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot go forward"));
}

void AiOverlayTools::ReloadPage(ReloadPageCallback callback) {
  RecordToolCallInvoked("ReloadPage");
  content::WebContents* contents = GetActiveWebContents();
  if (contents) {
    contents->GetController().Reload(content::ReloadType::NORMAL, true);
    std::move(callback).Run(std::monostate());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot reload page"));
}

AiOverlayTools::AnnotationTask::AnnotationTask(
    mojo::PendingReceiver<blink::mojom::AnnotationAgentHost> host_receiver,
    mojo::Remote<blink::mojom::AnnotationAgent> agent_remote,
    FindAndHighlightCallback callback)
    : receiver_(this, std::move(host_receiver)),
      agent_remote_(std::move(agent_remote)),
      callback_(std::move(callback)) {}

AiOverlayTools::AnnotationTask::~AnnotationTask() {
  if (callback_) {
    std::move(callback_).Run(base::unexpected("Task destroyed"));
  }
}

void AiOverlayTools::AnnotationTask::DidFinishAttachment(
    const gfx::Rect& document_relative_rect,
    blink::mojom::AttachmentResult attachment_result) {
  if (attachment_result == blink::mojom::AttachmentResult::kSuccess) {
    agent_remote_->ScrollIntoView(/*applies_focus=*/true);
    if (callback_) {
      std::move(callback_).Run(std::monostate());
    }
  } else {
    if (callback_) {
      std::move(callback_).Run(base::unexpected("No match found"));
    }
  }
}

void AiOverlayTools::OnAnnotationAgentDisconnected() {
  annotation_task_.reset();
}

void AiOverlayTools::FindAndHighlight(const std::string& query,
                                      FindAndHighlightCallback callback) {
  RecordToolCallInvoked("FindAndHighlight");
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }
  if (!contents->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(base::unexpected(
        "FindAndHighlight is only supported on HTTP/HTTPS pages"));
    return;
  }

  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (annotation_document_.AsRenderFrameHostIfValid() != rfh) {
    annotation_container_.reset();
    annotation_task_.reset();
    annotation_document_ = rfh->GetWeakDocumentPtr();
    rfh->GetRemoteInterfaces()->GetInterface(
        annotation_container_.BindNewPipeAndPassReceiver());
  }

  mojo::PendingRemote<blink::mojom::AnnotationAgentHost> host_remote;
  mojo::Remote<blink::mojom::AnnotationAgent> agent_remote;

  auto agent_receiver = agent_remote.BindNewPipeAndPassReceiver();
  annotation_task_ = std::make_unique<AnnotationTask>(
      host_remote.InitWithNewPipeAndPassReceiver(), std::move(agent_remote),
      std::move(callback));

  // Use a text fragment selector for direct highlighting.
  auto selector = blink::mojom::Selector::NewSerializedSelector(
      url::EncodeUriComponent(query));

  annotation_container_->CreateAgent(
      std::move(host_remote), std::move(agent_receiver),
      blink::mojom::AnnotationType::kGlic, std::move(selector),
      /*search_range_start_node_id=*/std::nullopt);
}

void AiOverlayTools::PlayVideo(PlayVideoCallback callback) {
  RecordToolCallInvoked("PlayVideo");
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(contents);
  if (media_session) {
    media_session->Resume(content::MediaSession::SuspendType::kSystem);
    std::move(callback).Run(std::monostate());
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}

void AiOverlayTools::PauseVideo(PauseVideoCallback callback) {
  RecordToolCallInvoked("PauseVideo");
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(contents);
  if (media_session) {
    media_session->Suspend(content::MediaSession::SuspendType::kSystem);
    std::move(callback).Run(std::monostate());
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}

void AiOverlayTools::SeekToTimestamp(const std::string& timecode,
                                     SeekToTimestampCallback callback) {
  RecordToolCallInvoked("SeekToTimestamp");
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::MediaSession* media_session =
      content::MediaSession::GetIfExists(contents);
  if (media_session) {
    std::optional<base::TimeDelta> seek_time = ParseTimecode(timecode);
    if (seek_time.has_value() &&
        (seek_time.value().is_positive() || seek_time.value().is_zero())) {
      media_session->SeekTo(seek_time.value());
      std::move(callback).Run(std::monostate());
    } else {
      std::move(callback).Run(base::unexpected("Invalid timecode"));
    }
  } else {
    std::move(callback).Run(base::unexpected("No active media session"));
  }
}

void AiOverlayTools::TranslatePage(const std::string& target_language,
                                   TranslatePageCallback callback) {
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  ChromeTranslateClient* translate_client =
      ChromeTranslateClient::FromWebContents(contents);
  if (!translate_client) {
    std::move(callback).Run(base::unexpected("Translation not supported"));
    return;
  }

  translate::TranslateManager* translate_manager =
      translate_client->GetTranslateManager();
  if (!translate_manager) {
    std::move(callback).Run(base::unexpected("Translation not available"));
    return;
  }

  if (target_language.empty()) {
    translate_manager->ShowTranslateUI(/*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  } else {
    if (!translate::TranslateDownloadManager::IsSupportedLanguage(
            target_language)) {
      std::move(callback).Run(base::unexpected("Unsupported language"));
      return;
    }

    translate_manager->ShowTranslateUI(std::nullopt, target_language,
                                       /*auto_translate=*/true,
                                       /*triggered_from_menu=*/true);
  }

  std::move(callback).Run(std::monostate());
}

void AiOverlayTools::AddBookmark(AddBookmarkCallback callback) {
  RecordToolCallInvoked("AddBookmark");
  Profile* profile = browser_->GetProfile();
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!bookmark_model || !bookmark_model->loaded()) {
    std::move(callback).Run(base::unexpected("Bookmark model not loaded"));
    return;
  }

  content::WebContents* active_contents = GetActiveWebContents();
  if (!active_contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  const GURL& target_url = active_contents->GetVisibleURL();
  const std::u16string& title = active_contents->GetTitle();

  const bookmarks::BookmarkNode* other_node = bookmark_model->other_node();
  bookmark_model->AddNewURL(other_node, other_node->children().size(), title,
                            target_url);
  std::move(callback).Run(std::monostate());
}

void AiOverlayTools::RemoveBookmark(RemoveBookmarkCallback callback) {
  RecordToolCallInvoked("RemoveBookmark");
  Profile* profile = browser_->GetProfile();
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (!bookmark_model || !bookmark_model->loaded()) {
    std::move(callback).Run(base::unexpected("Bookmark model not loaded"));
    return;
  }

  content::WebContents* active_contents = GetActiveWebContents();
  if (!active_contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  const GURL& target_url = active_contents->GetVisibleURL();
  std::vector<raw_ptr<const bookmarks::BookmarkNode, VectorExperimental>>
      nodes = bookmark_model->GetNodesByURL(target_url);
  if (nodes.empty()) {
    std::move(callback).Run(base::unexpected("Active tab is not bookmarked"));
    return;
  }

  for (const auto& node : nodes) {
    bookmark_model->Remove(node.get(),
                           bookmarks::metrics::BookmarkEditSource::kUser,
                           FROM_HERE);
  }
  std::move(callback).Run(std::monostate());
}

void AiOverlayTools::SetText(const blink::DOMNodeIdType& dom_node_id,
                             const std::string& text,
                             SetTextCallback callback) {
  RecordToolCallInvoked("SetText");
  // TODO(crbug.com/540575255): Scope form editing actions to the target
  // WebContents active when tool invocation was requested.
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  // TODO(crbug.com/540584855): Support targeting subframes/iframes when Page
  // Content Summary includes iframe DOM node IDs.
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (!rfh) {
    std::move(callback).Run(base::unexpected("No main frame"));
    return;
  }

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame);

  auto type_action = actor::mojom::TypeAction::New();
  type_action->mode = actor::mojom::TypeAction::Mode::kDeleteExisting;
  type_action->text = text;
  type_action->follow_by_enter = false;

  auto invocation = actor::mojom::ToolInvocation::New();
  invocation->task_id = actor::TaskId();
  invocation->target =
      actor::mojom::ToolTarget::NewDomNodeId(dom_node_id.value());
  invocation->action =
      actor::mojom::ToolAction::NewType(std::move(type_action));

  auto* raw_frame = chrome_render_frame.get();
  raw_frame->InvokeTool(
      std::move(invocation),
      base::BindOnce(
          [](mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> remote,
             SetTextCallback cb, actor::mojom::ActionResultPtr res) {
            if (res && res->code == actor::mojom::ActionResultCode::kOk) {
              std::move(cb).Run(base::ok(std::monostate()));
            } else {
              std::move(cb).Run(base::unexpected(
                  res ? res->message : "Input field not found or hidden"));
            }
          },
          std::move(chrome_render_frame), std::move(callback)));
}

void AiOverlayTools::ClickElement(const blink::DOMNodeIdType& dom_node_id,
                                  ClickElementCallback callback) {
  RecordToolCallInvoked("ClickElement");
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (!rfh) {
    std::move(callback).Run(base::unexpected("No main frame"));
    return;
  }

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame);

  auto click_action = actor::mojom::ClickAction::New();
  click_action->type = actor::mojom::ClickType::kLeft;
  click_action->count = actor::mojom::ClickCount::kSingle;

  auto invocation = actor::mojom::ToolInvocation::New();
  invocation->task_id = actor::TaskId();
  invocation->target =
      actor::mojom::ToolTarget::NewDomNodeId(dom_node_id.value());
  invocation->action =
      actor::mojom::ToolAction::NewClick(std::move(click_action));

  auto* raw_frame = chrome_render_frame.get();
  raw_frame->InvokeTool(
      std::move(invocation),
      base::BindOnce(
          [](mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> remote,
             ClickElementCallback cb, actor::mojom::ActionResultPtr res) {
            if (res && res->code == actor::mojom::ActionResultCode::kOk) {
              std::move(cb).Run(base::ok(std::monostate()));
            } else {
              std::move(cb).Run(base::unexpected(
                  (res && !res->message.empty())
                      ? res->message
                      : "Element not found or non-clickable"));
            }
          },
          std::move(chrome_render_frame), std::move(callback)));
}

void AiOverlayTools::SelectOption(const blink::DOMNodeIdType& dom_node_id,
                                  const std::string& value,
                                  SelectOptionCallback callback) {
  RecordToolCallInvoked("SelectOption");
  content::WebContents* contents = GetActiveWebContents();
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }

  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (!rfh) {
    std::move(callback).Run(base::unexpected("No main frame"));
    return;
  }

  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  rfh->GetRemoteAssociatedInterfaces()->GetInterface(&chrome_render_frame);

  auto select_action = actor::mojom::SelectAction::New();
  select_action->value = value;

  auto invocation = actor::mojom::ToolInvocation::New();
  invocation->task_id = actor::TaskId();
  invocation->target =
      actor::mojom::ToolTarget::NewDomNodeId(dom_node_id.value());
  invocation->action =
      actor::mojom::ToolAction::NewSelect(std::move(select_action));

  auto* raw_frame = chrome_render_frame.get();
  raw_frame->InvokeTool(
      std::move(invocation),
      base::BindOnce(
          [](mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> remote,
             SelectOptionCallback cb, actor::mojom::ActionResultPtr res) {
            if (res && res->code == actor::mojom::ActionResultCode::kOk) {
              std::move(cb).Run(base::ok(std::monostate()));
            } else {
              std::move(cb).Run(base::unexpected(
                  res ? res->message : "Dropdown element or option value not found"));
            }
          },
          std::move(chrome_render_frame), std::move(callback)));
}

void AiOverlayTools::GetToolDefinitions(GetToolDefinitionsCallback callback) {
  std::move(callback).Run(kBuiltInToolDefinitionsJson);
}

}  // namespace ttc
