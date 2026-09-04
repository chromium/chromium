// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools.h"

#include <algorithm>
#include <optional>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/glic/public/glic_instance.h"
#include "chrome/browser/glic/public/glic_invoke_options.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/public/glic_passkeys.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/translate/chrome_translate_client.h"
#include "chrome/browser/ttc/tool_controller.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/generated_tool_definitions.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#endif
#include "chrome/browser/ui/webui/ai_overlay_dialog/page_context_monitor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/translate/core/browser/translate_download_manager.h"
#include "components/translate/core/browser/translate_manager.h"
#include "content/public/browser/media_session.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "ui/base/base_window.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/events/types/scroll_types.h"
#include "url/url_util.h"

namespace {

content::WebContents* GetActiveWebContentsFromBrowser(
    BrowserWindowInterface* browser) {
#if !BUILDFLAG(IS_ANDROID)
  return browser->GetTabStripModel()->GetActiveWebContents();
#else
  return nullptr;
#endif
}

void RecordToolCallInvoked(std::string_view tool_name) {
  base::UmaHistogramBoolean(
      base::StrCat({"AI.OverlayDialog.ToolCallInvoked.", tool_name}), true);
}

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

#if !BUILDFLAG(IS_ANDROID)
std::string FormatDate(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf("%04d-%02d-%02d", exploded.year, exploded.month,
                            exploded.day_of_month);
}
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

namespace ttc {

AiOverlayTools::AiOverlayTools(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser,
    PageContextMonitor* page_context_monitor)
    : receiver_(this, std::move(receiver)),
      browser_(browser),
      page_context_monitor_(page_context_monitor) {
  if (features::kAiOverlayDialogUsesActor.Get()) {
    tool_controller_ = std::make_unique<ToolController>(browser_->GetProfile());
  }
}

AiOverlayTools::~AiOverlayTools() = default;

void AiOverlayTools::BindRegistryReceiver(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayToolRegistry>
        registry_receiver) {
  registry_receiver_.Bind(std::move(registry_receiver));
}

void AiOverlayTools::OpenUrl(const std::string& url_string,
                             bool new_tab,
                             OpenUrlCallback callback) {
  RecordToolCallInvoked("OpenUrl");
  if (tool_controller_) {
    tool_controller_->OpenUrl(browser_, url_string, new_tab,
                              std::move(callback));
    return;
  }

  GURL url(url_string);
  if (!url.is_valid()) {
    std::move(callback).Run(base::unexpected("Invalid URL"));
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  WindowOpenDisposition disposition =
      new_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
              : WindowOpenDisposition::CURRENT_TAB;
  browser_->OpenGURL(url, disposition);
  std::move(callback).Run(std::monostate());
#else
  std::move(callback).Run(base::unexpected("OpenUrl not yet ported to Clank"));
#endif
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

void AiOverlayTools::SwitchTab(const std::string& query,
                               SwitchTabCallback callback) {
  RecordToolCallInvoked("SwitchTab");
#if !BUILDFLAG(IS_ANDROID)
  std::string query_lower = base::ToLowerASCII(query);
  TabStripModel* tab_strip_model = browser_->GetTabStripModel();
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    std::string title =
        base::ToLowerASCII(base::UTF16ToUTF8(contents->GetTitle()));
    std::string url = base::ToLowerASCII(contents->GetURL().spec());
    if (title.find(query_lower) != std::string::npos ||
        url.find(query_lower) != std::string::npos) {
      tab_strip_model->ActivateTabAt(i);
      browser_->GetWindow()->Activate();

      auto result = ai_overlay_dialog::mojom::SwitchTabResult::New();
      result->title = base::UTF16ToUTF8(contents->GetTitle());
      result->url = contents->GetURL();
      result->tab_id = sessions::SessionTabHelper::IdForTab(contents).id();

      std::move(callback).Run(std::move(result));
      return;
    }
  }
  std::move(callback).Run(base::unexpected("No matching tab found"));
#else
  std::move(callback).Run(
      base::unexpected("SwitchTab not yet ported to Clank"));
#endif
}

void AiOverlayTools::CloseCurrentTab(CloseCurrentTabCallback callback) {
  RecordToolCallInvoked("CloseCurrentTab");
#if !BUILDFLAG(IS_ANDROID)
  if (browser_->GetTabStripModel()->count() > 0) {
    browser_->GetTabStripModel()->CloseSelectedTabs();
    std::move(callback).Run(std::monostate());
  } else {
    std::move(callback).Run(base::unexpected("No active tab to close"));
  }
#else
  std::move(callback).Run(
      base::unexpected("CloseCurrentTab not yet ported to Clank"));
#endif
}

void AiOverlayTools::GoBack(GoBackCallback callback) {
  RecordToolCallInvoked("GoBack");
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
  if (contents && contents->GetController().CanGoBack()) {
    contents->GetController().GoBack();
    std::move(callback).Run(std::monostate());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot go back"));
}

void AiOverlayTools::GoForward(GoForwardCallback callback) {
  RecordToolCallInvoked("GoForward");
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
  if (contents && contents->GetController().CanGoForward()) {
    contents->GetController().GoForward();
    std::move(callback).Run(std::monostate());
    return;
  }
  std::move(callback).Run(base::unexpected("Cannot go forward"));
}

void AiOverlayTools::ReloadPage(ReloadPageCallback callback) {
  RecordToolCallInvoked("ReloadPage");
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
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
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
  if (!contents) {
    std::move(callback).Run(base::unexpected("No active tab"));
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

void AiOverlayTools::Scroll(
    ai_overlay_dialog::mojom::ScrollGranularity granularity,
    double magnitude,
    ScrollCallback callback) {
  RecordToolCallInvoked("Scroll");
#if !BUILDFLAG(IS_ANDROID)
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
  if (!contents || !contents->GetRenderWidgetHostView()) {
    std::move(callback).Run(base::unexpected("No active tab or view"));
    return;
  }

  content::RenderWidgetHost* widget_host =
      contents->GetRenderWidgetHostView()->GetRenderWidgetHost();

  auto get_key_code =
      [](ai_overlay_dialog::mojom::ScrollGranularity granularity,
         double magnitude) {
        switch (granularity) {
          case ai_overlay_dialog::mojom::ScrollGranularity::kPage:
            return (magnitude > 0) ? ui::VKEY_NEXT : ui::VKEY_PRIOR;
          case ai_overlay_dialog::mojom::ScrollGranularity::kDocument:
            return (magnitude > 0) ? ui::VKEY_END : ui::VKEY_HOME;
        }
      };

  ui::KeyboardCode key_code = get_key_code(granularity, magnitude);

  // For Document granularity, we only need to send the key once to reach the
  // end or start. For Page granularity, we send it for each page requested.
  ui::KeyEvent pressed_event(ui::EventType::kKeyPressed, key_code, ui::EF_NONE);
  ui::KeyEvent released_event(ui::EventType::kKeyReleased, key_code,
                              ui::EF_NONE);
  widget_host->ForwardKeyboardEvent(
      input::NativeWebKeyboardEvent(pressed_event));
  widget_host->ForwardKeyboardEvent(
      input::NativeWebKeyboardEvent(released_event));

  std::move(callback).Run(std::monostate());
#else
  NOTIMPLEMENTED();
  std::move(callback).Run(base::unexpected("Scroll not yet ported to Clank"));
#endif
}

void AiOverlayTools::PlayVideo(PlayVideoCallback callback) {
  RecordToolCallInvoked("PlayVideo");
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
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
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
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

void AiOverlayTools::OpenGeminiPanel(const std::string& prompt,
                                     OpenGeminiPanelCallback callback) {
  RecordToolCallInvoked("OpenGeminiPanel");
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          browser_->GetProfile());

  if (!glic_service) {
    std::move(callback).Run(base::unexpected("Glic service not available"));
    return;
  }

#if !BUILDFLAG(IS_ANDROID)
  auto* active_tab = browser_->GetTabStripModel()->GetActiveTab();
#else
  tabs::TabInterface* active_tab = nullptr;
#endif
  if (!active_tab) {
    std::move(callback).Run(base::unexpected("No active tab"));
    return;
  }
  glic::GlicInvokeOptions options(glic::Target(*active_tab),
                                  glic::mojom::InvocationSource::kOsButton);
  options.prompts.push_back(prompt);

  auto split_callback = base::SplitOnceCallback(std::move(callback));

  options.on_success = base::BindOnce(
      [](OpenGeminiPanelCallback cb) {
        std::move(cb).Run(base::ok("Gemini panel opened."));
      },
      std::move(split_callback.first));

  options.on_error = base::BindOnce(
      [](OpenGeminiPanelCallback cb, glic::GlicInvokeError error) {
        std::move(cb).Run(base::unexpected("Failed to open Gemini panel"));
      },
      std::move(split_callback.second));

  glic_service->InvokeWithAutoSubmit(
      glic::InvokeWithAutoSubmitPasskeyProvider::GetPassKey(),
      std::move(options));
}

void AiOverlayTools::SeekToTimestamp(const std::string& timecode,
                                     SeekToTimestampCallback callback) {
  RecordToolCallInvoked("SeekToTimestamp");
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
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
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
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

  content::WebContents* active_contents =
      GetActiveWebContentsFromBrowser(browser_);
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

  content::WebContents* active_contents =
      GetActiveWebContentsFromBrowser(browser_);
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

namespace {

#if !BUILDFLAG(IS_ANDROID)

bool HasOpenTabWithUrl(const TabStripModel& tab_strip, const GURL& url) {
  // TODO(crbug.com/540589868): Consider parameter/fragment-insensitive URL
  // comparison to avoid duplicate candidates when session tokens differ.
  for (int i = 0; i < tab_strip.count(); ++i) {
    content::WebContents* contents = tab_strip.GetWebContentsAt(i);
    if (contents && contents->GetLastCommittedURL() == url) {
      return true;
    }
  }
  return false;
}

void FinishOpenPage(base::DictValue response,
                    TabStripModel& tab_strip,
                    AiOverlayTools::OpenPageCallback callback) {
  const base::ListValue* open_tabs = response.FindList("open_tabs");
  const base::ListValue* bookmarks = response.FindList("bookmarks");
  const base::ListValue* history = response.FindList("history");

  size_t open_tabs_count = open_tabs ? open_tabs->size() : 0;
  size_t bookmarks_count = bookmarks ? bookmarks->size() : 0;
  size_t history_count = history ? history->size() : 0;
  size_t total_matches = open_tabs_count + bookmarks_count + history_count;

  if (total_matches == 1) {
    if (open_tabs_count == 1) {
      const base::DictValue& tab_dict = (*open_tabs)[0].GetDict();
      int tab_id = tab_dict.FindInt("tab_id").value_or(0);
      const std::string* title = tab_dict.FindString("title");

      tab_strip.ActivateTabAt(tab_id);
      base::DictValue auto_response;
      auto_response.Set("action", "switched_tab");
      auto_response.Set("tab_id", tab_id);
      if (title) {
        auto_response.Set("title", *title);
      }
      std::optional<std::string> auto_json = base::WriteJson(auto_response);
      if (!auto_json) {
        std::move(callback).Run(
            base::unexpected("Failed to format search result response JSON"));
        return;
      }
      std::move(callback).Run(base::ok(std::move(*auto_json)));
      return;
    }

    if (bookmarks_count == 1) {
      const base::DictValue& bm_dict = (*bookmarks)[0].GetDict();
      const std::string* url_str = bm_dict.FindString("url");
      const std::string* title = bm_dict.FindString("title");

      content::WebContents* active_contents = tab_strip.GetActiveWebContents();
      if (active_contents && url_str) {
        active_contents->GetController().LoadURL(
            GURL(*url_str), content::Referrer(),
            ui::PAGE_TRANSITION_AUTO_BOOKMARK, std::string());
      }
      base::DictValue auto_response;
      auto_response.Set("action", "opened_bookmark");
      if (title) {
        auto_response.Set("title", *title);
      }
      std::optional<std::string> auto_json = base::WriteJson(auto_response);
      if (!auto_json) {
        std::move(callback).Run(
            base::unexpected("Failed to format search result response JSON"));
        return;
      }
      std::move(callback).Run(base::ok(std::move(*auto_json)));
      return;
    }

    if (history_count == 1) {
      const base::DictValue& hist_dict = (*history)[0].GetDict();
      const std::string* url_str = hist_dict.FindString("url");
      const std::string* title = hist_dict.FindString("title");

      content::WebContents* active_contents = tab_strip.GetActiveWebContents();
      if (active_contents && url_str) {
        active_contents->GetController().LoadURL(
            GURL(*url_str), content::Referrer(),
            ui::PAGE_TRANSITION_TYPED, std::string());
      }
      base::DictValue auto_response;
      auto_response.Set("action", "opened_history");
      if (title) {
        auto_response.Set("title", *title);
      }
      std::optional<std::string> auto_json = base::WriteJson(auto_response);
      if (!auto_json) {
        std::move(callback).Run(
            base::unexpected("Failed to format search result response JSON"));
        return;
      }
      std::move(callback).Run(base::ok(std::move(*auto_json)));
      return;
    }
  }

  std::optional<std::string> json_str = base::WriteJson(response);
  if (!json_str) {
    std::move(callback).Run(
        base::unexpected("Failed to format search results response JSON"));
    return;
  }
  std::move(callback).Run(base::ok(std::move(*json_str)));
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace

void AiOverlayTools::OpenPage(const std::string& query,
                              OpenPageCallback callback) {
  RecordToolCallInvoked("OpenPage");
#if !BUILDFLAG(IS_ANDROID)
  TabStripModel* tab_strip = browser_ ? browser_->GetTabStripModel() : nullptr;
  if (!tab_strip) {
    std::move(callback).Run(base::unexpected("No tab strip model available"));
    return;
  }

  Profile* profile = browser_->GetProfile();
  std::string lower_query = base::ToLowerASCII(query);

  base::DictValue response;
  int target_id_counter = 1;

  // 1. Search Open Tabs
  base::ListValue open_tabs_list;
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* contents = tab_strip->GetWebContentsAt(i);
    if (!contents) continue;

    std::string title = base::UTF16ToUTF8(contents->GetTitle());
    std::string url_str = contents->GetVisibleURL().spec();
    if (base::ToLowerASCII(title).find(lower_query) != std::string::npos ||
        base::ToLowerASCII(url_str).find(lower_query) != std::string::npos) {
      base::DictValue tab_dict;
      tab_dict.Set("target_id", target_id_counter++);
      tab_dict.Set("title", title);
      tab_dict.Set("url", url_str);
      tab_dict.Set("tab_id", i);
      open_tabs_list.Append(std::move(tab_dict));
    }
  }
  response.Set("open_tabs", std::move(open_tabs_list));

  // 2. Search Bookmarks
  base::ListValue bookmarks_list;
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  if (bookmark_model && bookmark_model->loaded()) {
    bookmarks::QueryFields query_fields;
    query_fields.word_phrase_query =
        std::make_unique<std::u16string>(base::UTF8ToUTF16(query));
    std::vector<const bookmarks::BookmarkNode*> matches =
        bookmarks::GetBookmarksMatchingProperties(bookmark_model, query_fields, 10);

    for (const auto* node : matches) {
      base::DictValue bm_dict;
      bm_dict.Set("target_id", target_id_counter++);
      bm_dict.Set("title", node->GetTitle());
      bm_dict.Set("url", node->url().spec());
      if (node->parent()) {
        bm_dict.Set("folder", node->parent()->GetTitle());
      }
      bm_dict.Set("date_added", FormatDate(node->date_added()));
      bookmarks_list.Append(std::move(bm_dict));
    }
  }
  response.Set("bookmarks", std::move(bookmarks_list));

  // 3. Search History
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfile(profile,
                                            ServiceAccessType::EXPLICIT_ACCESS);
  if (!history_service) {
    FinishOpenPage(std::move(response), *tab_strip, std::move(callback));
    return;
  }

  history::QueryOptions options;
  options.max_count = 10;
  history_service->QueryHistory(
      base::UTF8ToUTF16(query), options,
      base::BindOnce(
          [](base::DictValue res, OpenPageCallback cb,
             base::WeakPtr<AiOverlayTools> self,
             int start_target_id, history::QueryResults results) {
            if (!self || !self->browser_) {
              std::move(cb).Run(base::unexpected("Browser closed"));
              return;
            }
            TabStripModel* tab_strip = self->browser_->GetTabStripModel();
            CHECK(tab_strip);
            base::ListValue history_list;
            int current_target_id = start_target_id;

            for (const history::URLResult& result : results) {
              if (HasOpenTabWithUrl(*tab_strip, result.url())) {
                continue;
              }

              base::DictValue hist_dict;
              hist_dict.Set("target_id", current_target_id++);
              hist_dict.Set("title", result.title());
              hist_dict.Set("url", result.url().spec());
              hist_dict.Set("date_visited", FormatDate(result.visit_time()));
              history_list.Append(std::move(hist_dict));
            }
            res.Set("history", std::move(history_list));

            FinishOpenPage(std::move(res), *tab_strip, std::move(cb));
          },
          std::move(response), std::move(callback), weak_factory_.GetWeakPtr(),
          target_id_counter),
      &task_tracker_);
#else
  std::move(callback).Run(base::unexpected("OpenPage not yet ported to Clank"));
#endif
}

void AiOverlayTools::SetText(const blink::DOMNodeIdType& dom_node_id,
                             const std::string& text,
                             SetTextCallback callback) {
  RecordToolCallInvoked("SetText");
  // TODO(crbug.com/540575255): Scope form editing actions to the target
  // WebContents active when tool invocation was requested.
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
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
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
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

void AiOverlayTools::SetFullscreen(bool fullscreen,
                                   SetFullscreenCallback callback) {
  RecordToolCallInvoked("SetFullscreen");
#if !BUILDFLAG(IS_ANDROID)
  if (!browser_ || !browser_->GetWindow()) {
    std::move(callback).Run(base::unexpected("No active browser window"));
    return;
  }
  bool is_fullscreen = browser_->GetWindow()->IsFullscreen();
  if (fullscreen != is_fullscreen) {
    chrome::ToggleFullscreenMode(browser_.get());
  }
  std::move(callback).Run(base::ok(std::monostate()));
#else
  std::move(callback).Run(
      base::unexpected("SetFullscreen not yet ported to Clank"));
#endif
}

void AiOverlayTools::SelectOption(const blink::DOMNodeIdType& dom_node_id,
                                  const std::string& value,
                                  SelectOptionCallback callback) {
  RecordToolCallInvoked("SelectOption");
  content::WebContents* contents = GetActiveWebContentsFromBrowser(browser_);
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
