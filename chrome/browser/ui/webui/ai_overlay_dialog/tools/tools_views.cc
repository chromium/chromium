// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ai_overlay_dialog/tools/tools_views.h"

#include <optional>
#include <string>
#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
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
#include "chrome/browser/glic/public/service/glic_instance_coordinator.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ttc/core/session_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/history/core/browser/history_service.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/base_window.h"
#include "ui/base/window_open_disposition.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

namespace ttc {

namespace {

std::string FormatDate(base::Time time) {
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  return base::StringPrintf("%04d-%02d-%02d", exploded.year, exploded.month,
                            exploded.day_of_month);
}

bool HasOpenTabWithUrl(const TabStripModel& tab_strip, const GURL& url) {
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
            GURL(*url_str), content::Referrer(), ui::PAGE_TRANSITION_TYPED,
            std::string());
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

}  // namespace

// static
std::unique_ptr<AiOverlayTools> AiOverlayTools::Create(
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser,
    PageContextMonitor* page_context_monitor) {
  return std::make_unique<AiOverlayToolsViews>(base::PassKey<AiOverlayTools>(),
                                               std::move(receiver), browser,
                                               page_context_monitor);
}

AiOverlayToolsViews::AiOverlayToolsViews(
    base::PassKey<AiOverlayTools>,
    mojo::PendingReceiver<ai_overlay_dialog::mojom::AiOverlayTools> receiver,
    BrowserWindowInterface* browser,
    PageContextMonitor* page_context_monitor)
    : AiOverlayTools(std::move(receiver), browser, page_context_monitor) {}

AiOverlayToolsViews::~AiOverlayToolsViews() = default;

content::WebContents* AiOverlayToolsViews::GetActiveWebContents() const {
  if (!browser() || !browser()->GetTabStripModel()) {
    return nullptr;
  }
  return browser()->GetTabStripModel()->GetActiveWebContents();
}

void AiOverlayToolsViews::OpenUrl(const std::string& url_string,
                                  bool new_tab,
                                  OpenUrlCallback callback) {
  RecordToolCallInvoked("OpenUrl");
  GURL url(url_string);
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    std::move(callback).Run(base::unexpected("Invalid URL"));
    return;
  }

  WindowOpenDisposition disposition =
      new_tab ? WindowOpenDisposition::NEW_FOREGROUND_TAB
              : WindowOpenDisposition::CURRENT_TAB;
  browser()->OpenGURL(url, disposition);
  std::move(callback).Run(std::monostate());
}

void AiOverlayToolsViews::SwitchTab(const std::string& query,
                                    SwitchTabCallback callback) {
  RecordToolCallInvoked("SwitchTab");
  std::string query_lower = base::ToLowerASCII(query);
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  if (!tab_strip_model) {
    std::move(callback).Run(base::unexpected("No tab strip model available"));
    return;
  }
  for (int i = 0; i < tab_strip_model->count(); ++i) {
    content::WebContents* contents = tab_strip_model->GetWebContentsAt(i);
    if (!contents || !contents->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
      continue;
    }
    std::string title =
        base::ToLowerASCII(base::UTF16ToUTF8(contents->GetTitle()));
    std::string url = base::ToLowerASCII(contents->GetURL().spec());
    if (title.find(query_lower) != std::string::npos ||
        url.find(query_lower) != std::string::npos) {
      tab_strip_model->ActivateTabAt(i);
      browser()->GetWindow()->Activate();

      auto result = ai_overlay_dialog::mojom::SwitchTabResult::New();
      result->title = base::UTF16ToUTF8(contents->GetTitle());
      result->url = contents->GetURL();
      result->tab_id = sessions::SessionTabHelper::IdForTab(contents).id();

      std::move(callback).Run(std::move(result));
      return;
    }
  }
  std::move(callback).Run(base::unexpected("No matching tab found"));
}

void AiOverlayToolsViews::CloseCurrentTab(CloseCurrentTabCallback callback) {
  RecordToolCallInvoked("CloseCurrentTab");
  TabStripModel* tab_strip_model = browser()->GetTabStripModel();
  if (tab_strip_model && tab_strip_model->count() > 0) {
    tab_strip_model->CloseSelectedTabs();
    std::move(callback).Run(std::monostate());
  } else {
    std::move(callback).Run(base::unexpected("No active tab to close"));
  }
}

void AiOverlayToolsViews::Scroll(
    ai_overlay_dialog::mojom::ScrollGranularity granularity,
    double magnitude,
    ScrollCallback callback) {
  RecordToolCallInvoked("Scroll");
  content::WebContents* contents = GetActiveWebContents();
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
}

void AiOverlayToolsViews::OpenPage(const std::string& query,
                                   OpenPageCallback callback) {
  RecordToolCallInvoked("OpenPage");
  TabStripModel* tab_strip =
      browser() ? browser()->GetTabStripModel() : nullptr;
  if (!tab_strip) {
    std::move(callback).Run(base::unexpected("No tab strip model available"));
    return;
  }

  Profile* profile = browser()->GetProfile();
  std::string lower_query = base::ToLowerASCII(query);

  base::DictValue response;
  int target_id_counter = 1;

  // 1. Search Open Tabs
  base::ListValue open_tabs_list;
  for (int i = 0; i < tab_strip->count(); ++i) {
    content::WebContents* contents = tab_strip->GetWebContentsAt(i);
    if (!contents || !contents->GetLastCommittedURL().SchemeIsHTTPOrHTTPS()) {
      continue;
    }

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
        bookmarks::GetBookmarksMatchingProperties(bookmark_model, query_fields,
                                                  10);

    for (const auto* node : matches) {
      if (!node->url().SchemeIsHTTPOrHTTPS()) {
        continue;
      }
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
             base::WeakPtr<AiOverlayToolsViews> self, int start_target_id,
             history::QueryResults results) {
            if (!self || !self->browser()) {
              std::move(cb).Run(base::unexpected("Browser closed"));
              return;
            }
            TabStripModel* tab_strip = self->browser()->GetTabStripModel();
            CHECK(tab_strip);
            base::ListValue history_list;
            int current_target_id = start_target_id;

            for (const history::URLResult& result : results) {
              if (!result.url().SchemeIsHTTPOrHTTPS() ||
                  HasOpenTabWithUrl(*tab_strip, result.url())) {
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
      &task_tracker());
}

void AiOverlayToolsViews::SetFullscreen(bool fullscreen,
                                        SetFullscreenCallback callback) {
  RecordToolCallInvoked("SetFullscreen");
  if (!browser() || !browser()->GetWindow()) {
    std::move(callback).Run(base::unexpected("No active browser window"));
    return;
  }
  bool is_fullscreen = browser()->GetWindow()->IsFullscreen();
  if (fullscreen != is_fullscreen) {
    chrome::ToggleFullscreenMode(browser());
  }
  std::move(callback).Run(base::ok(std::monostate()));
}

void AiOverlayToolsViews::OpenGeminiPanel(const std::string& prompt,
                                          OpenGeminiPanelCallback callback) {
  RecordToolCallInvoked("OpenGeminiPanel");
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          browser()->GetProfile());

  if (!glic_service) {
    std::move(callback).Run(base::unexpected("Glic service not available"));
    return;
  }

  auto* active_tab = browser()->GetTabStripModel()
                         ? browser()->GetTabStripModel()->GetActiveTab()
                         : nullptr;
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

  glic_service->InvokeWithAutoSubmit(GetGlicPassKey(), std::move(options));
}

void AiOverlayToolsViews::CloseGeminiPanel(CloseGeminiPanelCallback callback) {
  RecordToolCallInvoked("CloseGeminiPanel");
  glic::GlicKeyedService* glic_service =
      glic::GlicKeyedServiceFactory::GetGlicKeyedService(
          browser()->GetProfile());

  if (!glic_service) {
    std::move(callback).Run(base::unexpected("Glic service not available"));
    return;
  }

  if (glic_service->instance_coordinator().IsPanelShowingForBrowser(
          *browser())) {
    // TODO(gklassen): Use a dedicated invocation source for tool calls when
    // productionizing.
    glic_service->instance_coordinator().Toggle(
        browser(), /*prevent_close=*/false,
        glic::mojom::InvocationSource::kOsButton);
  }
  std::move(callback).Run(std::monostate());
}

}  // namespace ttc
