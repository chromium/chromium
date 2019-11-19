// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_menu_model.h"
#include "chrome/browser/ui/tabs/tab_network_state.h"
#include "chrome/browser/ui/tabs/tab_renderer_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/favicon_source.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/browser/ui/webui/tab_strip/tab_strip_ui_layout.h"
#include "chrome/browser/ui/webui/theme_handler.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/tab_strip_resources.h"
#include "chrome/grit/tab_strip_resources_map.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "content/public/common/url_constants.h"
#include "third_party/skia/include/core/SkImageEncoder.h"
#include "third_party/skia/include/core/SkStream.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/resources/grit/webui_resources.h"

namespace {

// Writes bytes to a std::vector that can be fetched. This is used to record the
// output of skia image encoding.
class BufferWStream : public SkWStream {
 public:
  BufferWStream() = default;
  ~BufferWStream() override = default;

  // Returns the output buffer by moving.
  std::vector<unsigned char> GetBuffer() { return std::move(result_); }

  // SkWStream:
  bool write(const void* buffer, size_t size) override {
    const unsigned char* bytes = reinterpret_cast<const unsigned char*>(buffer);
    result_.insert(result_.end(), bytes, bytes + size);
    return true;
  }

  size_t bytesWritten() const override { return result_.size(); }

 private:
  std::vector<unsigned char> result_;
};

std::string MakeDataURIForImage(base::span<const uint8_t> image_data,
                                base::StringPiece mime_subtype) {
  std::string result = "data:image/";
  result.append(mime_subtype.begin(), mime_subtype.end());
  result += ";base64,";
  result += base::Base64Encode(image_data);
  return result;
}

std::string EncodePNGAndMakeDataURI(gfx::ImageSkia image, float scale_factor) {
  const SkBitmap& bitmap = image.GetRepresentation(scale_factor).GetBitmap();
  BufferWStream stream;
  const bool encoding_succeeded =
      SkEncodeImage(&stream, bitmap, SkEncodedImageFormat::kPNG, 100);
  DCHECK(encoding_succeeded);
  return MakeDataURIForImage(
      base::as_bytes(base::make_span(stream.GetBuffer())), "png");
}

class WebUITabContextMenu : public ui::SimpleMenuModel::Delegate,
                            public TabMenuModel {
 public:
  WebUITabContextMenu(TabStripModel* tab_strip_model, int tab_index)
      : TabMenuModel(this, tab_strip_model, tab_index),
        tab_strip_model_(tab_strip_model),
        tab_index_(tab_index) {}
  ~WebUITabContextMenu() override = default;

  void ExecuteCommand(int command_id, int event_flags) override {
    DCHECK_LT(tab_index_, tab_strip_model_->count());
    tab_strip_model_->ExecuteContextMenuCommand(
        tab_index_, static_cast<TabStripModel::ContextMenuCommand>(command_id));
  }

 private:
  TabStripModel* const tab_strip_model_;
  const int tab_index_;
};

}  // namespace

class TabStripUIHandler : public content::WebUIMessageHandler,
                          public TabStripModelObserver {
 public:
  explicit TabStripUIHandler(Browser* browser, TabStripUI::Embedder* embedder)
      : browser_(browser),
        embedder_(embedder),
        thumbnail_tracker_(base::Bind(&TabStripUIHandler::HandleThumbnailUpdate,
                                      base::Unretained(this))) {}
  ~TabStripUIHandler() override = default;

  void OnJavascriptAllowed() override {
    browser_->tab_strip_model()->AddObserver(this);
  }

  void NotifyLayoutChanged() {
    if (!IsJavascriptAllowed())
      return;
    FireWebUIListener("layout-changed", embedder_->GetLayout().AsDictionary());
  }

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override {
    if (tab_strip_model->empty())
      return;

    switch (change.type()) {
      case TabStripModelChange::kInserted: {
        for (const auto& contents : change.GetInsert()->contents) {
          FireWebUIListener("tab-created",
                            GetTabData(contents.contents, contents.index));
        }
        break;
      }
      case TabStripModelChange::kRemoved: {
        for (const auto& contents : change.GetRemove()->contents) {
          FireWebUIListener("tab-removed",
                            base::Value(extensions::ExtensionTabUtil::GetTabId(
                                contents.contents)));
        }
        break;
      }
      case TabStripModelChange::kMoved: {
        auto* move = change.GetMove();
        FireWebUIListener(
            "tab-moved",
            base::Value(extensions::ExtensionTabUtil::GetTabId(move->contents)),
            base::Value(move->to_index));
        break;
      }
      case TabStripModelChange::kReplaced: {
        auto* replace = change.GetReplace();
        FireWebUIListener("tab-replaced",
                          base::Value(extensions::ExtensionTabUtil::GetTabId(
                              replace->old_contents)),
                          base::Value(extensions::ExtensionTabUtil::GetTabId(
                              replace->new_contents)));
        break;
      }
      case TabStripModelChange::kGroupChanged: {
        // Not yet implmented.
        break;
      }
      case TabStripModelChange::kSelectionOnly:
        // Multi-selection is not supported for touch.
        break;
    }

    if (selection.active_tab_changed()) {
      content::WebContents* new_contents = selection.new_contents;
      int index = selection.new_model.active();
      if (new_contents && index != TabStripModel::kNoTab) {
        FireWebUIListener(
            "tab-active-changed",
            base::Value(extensions::ExtensionTabUtil::GetTabId(new_contents)));
      }
    }
  }

  void TabChangedAt(content::WebContents* contents,
                    int index,
                    TabChangeType change_type) override {
    FireWebUIListener("tab-updated", GetTabData(contents, index));
  }

  void TabPinnedStateChanged(TabStripModel* tab_strip_model,
                             content::WebContents* contents,
                             int index) override {
    FireWebUIListener("tab-updated", GetTabData(contents, index));
  }

  void TabBlockedStateChanged(content::WebContents* contents,
                              int index) override {
    FireWebUIListener("tab-updated", GetTabData(contents, index));
  }

 protected:
  // content::WebUIMessageHandler:
  void RegisterMessages() override {
    web_ui()->RegisterMessageCallback(
        "getTabs",
        base::Bind(&TabStripUIHandler::HandleGetTabs, base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getThemeColors", base::Bind(&TabStripUIHandler::HandleGetThemeColors,
                                     base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "setThumbnailTracked",
        base::Bind(&TabStripUIHandler::HandleSetThumbnailTracked,
                   base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "closeContainer", base::Bind(&TabStripUIHandler::HandleCloseContainer,
                                     base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "showTabContextMenu",
        base::Bind(&TabStripUIHandler::HandleShowTabContextMenu,
                   base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "getLayout", base::Bind(&TabStripUIHandler::HandleGetLayout,
                                base::Unretained(this)));
  }

 private:
  base::DictionaryValue GetTabData(content::WebContents* contents, int index) {
    base::DictionaryValue tab_data;

    tab_data.SetBoolean("active",
                        browser_->tab_strip_model()->active_index() == index);
    tab_data.SetInteger("id", extensions::ExtensionTabUtil::GetTabId(contents));
    tab_data.SetInteger("index", index);

    TabRendererData tab_renderer_data =
        TabRendererData::FromTabInModel(browser_->tab_strip_model(), index);
    tab_data.SetBoolean("pinned", tab_renderer_data.pinned);
    tab_data.SetString("title", tab_renderer_data.title);
    tab_data.SetString("url", tab_renderer_data.visible_url.GetContent());

    if (!tab_renderer_data.favicon.isNull()) {
      tab_data.SetString("favIconUrl", EncodePNGAndMakeDataURI(
                                           tab_renderer_data.favicon,
                                           web_ui()->GetDeviceScaleFactor()));
      tab_data.SetBoolean("isDefaultFavicon",
                          tab_renderer_data.favicon.BackedBySameObjectAs(
                              favicon::GetDefaultFavicon().AsImageSkia()));
    } else {
      tab_data.SetBoolean("isDefaultFavicon", true);
    }
    tab_data.SetBoolean("showIcon", tab_renderer_data.show_icon);
    tab_data.SetInteger("networkState",
                        static_cast<int>(tab_renderer_data.network_state));
    tab_data.SetBoolean("shouldHideThrobber",
                        tab_renderer_data.should_hide_throbber);
    tab_data.SetBoolean("blocked", tab_renderer_data.blocked);
    tab_data.SetBoolean("crashed", tab_renderer_data.IsCrashed());
    // TODO(johntlee): Add the rest of TabRendererData

    auto alert_states = std::make_unique<base::ListValue>();
    for (const auto alert_state :
         chrome::GetTabAlertStatesForContents(contents)) {
      alert_states->Append(static_cast<int>(alert_state));
    }
    tab_data.SetList("alertStates", std::move(alert_states));

    return tab_data;
  }

  void HandleGetTabs(const base::ListValue* args) {
    AllowJavascript();
    const base::Value& callback_id = args->GetList()[0];

    base::ListValue tabs;
    TabStripModel* tab_strip_model = browser_->tab_strip_model();
    for (int i = 0; i < tab_strip_model->count(); ++i) {
      tabs.Append(GetTabData(tab_strip_model->GetWebContentsAt(i), i));
    }
    ResolveJavascriptCallback(callback_id, tabs);
  }

  void HandleGetThemeColors(const base::ListValue* args) {
    AllowJavascript();
    const base::Value& callback_id = args->GetList()[0];

    const ui::ThemeProvider& tp =
        ThemeService::GetThemeProviderForProfile(browser_->profile());

    // This should return an object of CSS variables to rgba values so that
    // the WebUI can use the CSS variables to color the tab strip
    base::DictionaryValue colors;
    colors.SetString("--tabstrip-background-color",
                     color_utils::SkColorToRgbaString(
                         tp.GetColor(ThemeProperties::COLOR_FRAME)));
    colors.SetString("--tabstrip-tab-background-color",
                     color_utils::SkColorToRgbaString(
                         tp.GetColor(ThemeProperties::COLOR_TOOLBAR)));
    colors.SetString("--tabstrip-tab-text-color",
                     color_utils::SkColorToRgbaString(
                         tp.GetColor(ThemeProperties::COLOR_TAB_TEXT)));
    colors.SetString("--tabstrip-tab-separator-color",
                     color_utils::SkColorToRgbaString(SkColorSetA(
                         tp.GetColor(ThemeProperties::COLOR_TAB_TEXT),
                         /* 16% opacity */ 0.16 * 255)));

    colors.SetString("--tabstrip-tab-loading-spinning-color",
                     color_utils::SkColorToRgbaString(tp.GetColor(
                         ThemeProperties::COLOR_TAB_THROBBER_SPINNING)));
    colors.SetString("--tabstrip-tab-waiting-spinning-color",
                     color_utils::SkColorToRgbaString(tp.GetColor(
                         ThemeProperties::COLOR_TAB_THROBBER_WAITING)));
    colors.SetString("--tabstrip-indicator-recording-color",
                     color_utils::SkColorToRgbaString(tp.GetColor(
                         ThemeProperties::COLOR_TAB_ALERT_RECORDING)));
    colors.SetString("--tabstrip-indicator-pip-color",
                     color_utils::SkColorToRgbaString(
                         tp.GetColor(ThemeProperties::COLOR_TAB_PIP_PLAYING)));
    colors.SetString("--tabstrip-indicator-capturing-color",
                     color_utils::SkColorToRgbaString(tp.GetColor(
                         ThemeProperties::COLOR_TAB_ALERT_CAPTURING)));
    colors.SetString("--tabstrip-tab-blocked-color",
                     color_utils::SkColorToRgbaString(
                         ui::NativeTheme::GetInstanceForWeb()->GetSystemColor(
                             ui::NativeTheme::kColorId_ProminentButtonColor)));
    colors.SetString("--tabstrip-focus-outline-color",
                     color_utils::SkColorToRgbaString(
                         ui::NativeTheme::GetInstanceForWeb()->GetSystemColor(
                             ui::NativeTheme::kColorId_FocusedBorderColor)));

    ResolveJavascriptCallback(callback_id, colors);
  }

  void HandleCloseContainer(const base::ListValue* args) {
    DCHECK(embedder_);
    embedder_->CloseContainer();
  }

  void HandleShowTabContextMenu(const base::ListValue* args) {
    int tab_id = 0;
    args->GetInteger(0, &tab_id);

    gfx::PointF point;
    {
      double x = 0;
      args->GetDouble(1, &x);
      double y = 0;
      args->GetDouble(2, &y);
      point = gfx::PointF(x, y);
    }

    TabStripModel* tab_strip_model = nullptr;
    int tab_index = -1;
    const bool got_tab = extensions::ExtensionTabUtil::GetTabById(
        tab_id, browser_->profile(), true /* include_incognito */, nullptr,
        &tab_strip_model, nullptr, &tab_index);
    DCHECK(got_tab);
    DCHECK_EQ(tab_strip_model, browser_->tab_strip_model());

    DCHECK(embedder_);
    embedder_->ShowContextMenuAtPoint(
        gfx::ToRoundedPoint(point),
        std::make_unique<WebUITabContextMenu>(tab_strip_model, tab_index));
  }

  void HandleGetLayout(const base::ListValue* args) {
    AllowJavascript();
    const base::Value& callback_id = args->GetList()[0];

    base::Value layout = embedder_->GetLayout().AsDictionary();
    ResolveJavascriptCallback(callback_id, layout);
  }

  void HandleSetThumbnailTracked(const base::ListValue* args) {
    AllowJavascript();

    int tab_id = 0;
    if (!args->GetInteger(0, &tab_id))
      return;

    const bool thumbnail_tracked = args->GetList()[1].GetBool();

    content::WebContents* tab = nullptr;
    if (!extensions::ExtensionTabUtil::GetTabById(tab_id, browser_->profile(),
                                                  true, &tab)) {
      // ID didn't refer to a valid tab.
      DVLOG(1) << "Invalid tab ID";
      return;
    }

    if (thumbnail_tracked)
      thumbnail_tracker_.AddTab(tab);
    else
      thumbnail_tracker_.RemoveTab(tab);
  }

  // Callback passed to |thumbnail_tracker_|. Called when a tab's thumbnail
  // changes, or when we start watching the tab.
  void HandleThumbnailUpdate(content::WebContents* tab,
                             ThumbnailTracker::CompressedThumbnailData image) {
    // Send base-64 encoded image to JS side.
    std::string data_uri =
        MakeDataURIForImage(base::make_span(image->data), "jpeg");

    const int tab_id = extensions::ExtensionTabUtil::GetTabId(tab);
    FireWebUIListener("tab-thumbnail-updated", base::Value(tab_id),
                      base::Value(data_uri));
  }

  Browser* const browser_;
  TabStripUI::Embedder* const embedder_;

  ThumbnailTracker thumbnail_tracker_;

  DISALLOW_COPY_AND_ASSIGN(TabStripUIHandler);
};

TabStripUI::TabStripUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  content::HostZoomMap::Get(web_ui->GetWebContents()->GetSiteInstance())
      ->SetZoomLevelForHostAndScheme(content::kChromeUIScheme,
                                     chrome::kChromeUITabStripHost, 0);

  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUITabStripHost);

  html_source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources chrome://test 'self';");

  std::string generated_path =
      "@out_folder@/gen/chrome/browser/resources/tab_strip/";

  for (size_t i = 0; i < kTabStripResourcesSize; ++i) {
    std::string path = kTabStripResources[i].name;
    if (path.rfind(generated_path, 0) == 0) {
      path = path.substr(generated_path.length());
    }
    html_source->AddResourcePath(path, kTabStripResources[i].value);
  }
  html_source->AddResourcePath("test_loader.js", IDR_WEBUI_JS_TEST_LOADER);
  html_source->AddResourcePath("test_loader.html", IDR_WEBUI_HTML_TEST_LOADER);

  html_source->SetDefaultResource(IDR_TAB_STRIP_HTML);

  // Add a load time string for the frame color to allow the tab strip to paint
  // a background color that matches the frame before any content loads
  const ui::ThemeProvider& tp =
      ThemeService::GetThemeProviderForProfile(profile);
  html_source->AddString("frameColor",
                         color_utils::SkColorToRgbaString(
                             tp.GetColor(ThemeProperties::COLOR_FRAME)));

  html_source->AddBoolean(
      "showDemoOptions",
      base::FeatureList::IsEnabled(features::kWebUITabStripDemoOptions));

  static constexpr LocalizedString kStrings[] = {
      {"tabListTitle", IDS_ACCNAME_TAB_LIST},
      {"closeTab", IDS_ACCNAME_CLOSE},
      {"defaultTabTitle", IDS_DEFAULT_TAB_TITLE},
      {"loadingTab", IDS_TAB_LOADING_TITLE},
      {"tabCrashed", IDS_TAB_AX_LABEL_CRASHED_FORMAT},
      {"tabNetworkError", IDS_TAB_AX_LABEL_NETWORK_ERROR_FORMAT},
      {"audioPlaying", IDS_TAB_AX_LABEL_AUDIO_PLAYING_FORMAT},
      {"usbConnected", IDS_TAB_AX_LABEL_USB_CONNECTED_FORMAT},
      {"bluetoothConnected", IDS_TAB_AX_LABEL_BLUETOOTH_CONNECTED_FORMAT},
      {"serialConnected", IDS_TAB_AX_LABEL_SERIAL_CONNECTED_FORMAT},
      {"mediaRecording", IDS_TAB_AX_LABEL_MEDIA_RECORDING_FORMAT},
      {"audioMuting", IDS_TAB_AX_LABEL_AUDIO_MUTING_FORMAT},
      {"tabCapturing", IDS_TAB_AX_LABEL_DESKTOP_CAPTURING_FORMAT},
      {"pipPlaying", IDS_TAB_AX_LABEL_PIP_PLAYING_FORMAT},
      {"desktopCapturing", IDS_TAB_AX_LABEL_DESKTOP_CAPTURING_FORMAT},
      {"vrPresenting", IDS_TAB_AX_LABEL_VR_PRESENTING},
  };
  AddLocalizedStringsBulk(html_source, kStrings, base::size(kStrings));
  html_source->UseStringsJs();

  content::WebUIDataSource::Add(profile, html_source);

  content::URLDataSource::Add(
      profile, std::make_unique<FaviconSource>(
                   profile, chrome::FaviconUrlFormat::kFavicon2));

  web_ui->AddMessageHandler(std::make_unique<ThemeHandler>());
}

TabStripUI::~TabStripUI() {}

void TabStripUI::Initialize(Browser* browser, Embedder* embedder) {
  content::WebUI* const web_ui = TabStripUI::web_ui();
  DCHECK_EQ(Profile::FromWebUI(web_ui), browser->profile());
  auto handler = std::make_unique<TabStripUIHandler>(browser, embedder);
  handler_ = handler.get();
  web_ui->AddMessageHandler(std::move(handler));
}

void TabStripUI::LayoutChanged() {
  handler_->NotifyLayoutChanged();
}
