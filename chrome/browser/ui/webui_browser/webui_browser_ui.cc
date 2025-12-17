// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"

#include "base/notimplemented.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_bar_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/tabs/tab_strip_api/tab_strip_service_feature.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/searchbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/realbox_handler.h"
#include "chrome/browser/ui/webui_browser/bookmark_bar_page_handler.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_extensions_container.h"
#include "chrome/browser/ui/webui_browser/webui_browser_page_handler.h"
#include "chrome/browser/ui/webui_browser/webui_browser_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/tab_strip_api_resources_map.h"
#include "chrome/grit/webui_browser_resources.h"
#include "chrome/grit/webui_browser_resources_map.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/guest_contents/browser/guest_contents_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/webui_util.h"

#if BUILDFLAG(IS_MAC)
#include "base/mac/mac_util.h"
#endif

namespace {

std::string SidePanelEntryIdToTitle(SidePanelEntryId id) {
  // TODO(webium): Ideally, the titles should be added to SIDE_PANEL_ENTRY_IDS
  // macros in chrome/browser/ui/views/side_panel/side_panel_entry_id.h, and
  // then this conversion function would be written in that same file,
  // analogously to the other functions it contains. But it appears that some
  // of the entry  ids there are stale and no longer have a matching generated
  // IDS_*_TITLE string that we can include.
  //
  // Since we are currently only explicitly supporting three side panel types,
  // we do the id to title conversion here manually to minimize modification to
  // existing code.
  switch (id) {
    case SidePanelEntryId::kCustomizeChrome:
      return l10n_util::GetStringUTF8(IDS_SIDE_PANEL_CUSTOMIZE_CHROME_TITLE);
    case SidePanelEntryId::kReadingList:
      return l10n_util::GetStringUTF8(IDS_READ_LATER_TITLE);
    case SidePanelEntryId::kBookmarks:
      return l10n_util::GetStringUTF8(IDS_BOOKMARK_MANAGER_TITLE);
    default:
      NOTREACHED();
  }
}

}  // namespace

WebUIBrowserUIConfig::WebUIBrowserUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIWebuiBrowserHost) {}

bool WebUIBrowserUIConfig::IsWebUIEnabled(
    content::BrowserContext* browser_context) {
  return webui_browser::IsWebUIBrowserEnabled();
}

// realbox uses ui/webui/resources/js/metrics_reporter, which in turn uses
// chrome.timeTicks.nowInMicroseconds(). In order to provide it we pass
// enable_chrome_send to MojoWebUIController constructor, since they're
// a package deal via BindingsPolicyValue::kWebUi.
WebUIBrowserUI::WebUIBrowserUI(content::WebUI* web_ui)
    : ui::MojoWebUIController(web_ui, /*enable_chrome_send=*/true) {
  WebUIBrowserWindow* webui_browser_window =
      WebUIBrowserWindow::FromWebShellWebContents(web_ui->GetWebContents());
  browser_ = webui_browser_window->browser();

  // Set up the chrome://webui-browser source.
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      web_ui->GetWebContents()->GetBrowserContext(),
      chrome::kChromeUIWebuiBrowserHost);

  // Add required resources.
  webui::SetupWebUIDataSource(source, kWebuiBrowserResources,
                              IDR_WEBUI_BROWSER_WEBUI_BROWSER_HTML);
  source->AddResourcePaths(kTabStripApiResources);

  static constexpr webui::LocalizedString kStrings[] = {
      // Localized strings (alphabetical order).
      {"appMenuTooltip", IDS_APPMENU_TOOLTIP},
      {"tooltipExtensionsButton", IDS_TOOLTIP_EXTENSIONS_BUTTON},
      {"tooltipNewTab", IDS_TOOLTIP_NEW_TAB},
      {"tooltipReload", IDS_TOOLTIP_RELOAD},
      {"tooltipStop", IDS_TOOLTIP_STOP},
  };
  source->AddLocalizedStrings(kStrings);

  SearchboxHandler::SetupWebUIDataSource(source, Profile::FromWebUI(web_ui));
  source->AddBoolean("composeboxContextDragAndDropEnabled", false);
  source->AddBoolean("expandedSearchboxShowVoiceSearch", false);

  // TODO(crbug.com/445510209): Uncomment after installing WebUIOmniboxHandler.
  // source->AddBoolean("reportMetrics", true);
  // source->AddString("charTypedToPaintMetricName",
  //                   "Omnibox.WebUI.CharTypedToRepaintLatency.ToPaint");
  // source->AddString(
  //     "resultChangedToPaintMetricName",
  //     "Omnibox.Popup.WebUI.ResultChangedToRepaintLatency.ToPaint");
}

WebUIBrowserUI::~WebUIBrowserUI() = default;

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<webui_browser::mojom::PageHandlerFactory> receiver) {
  page_factory_receiver_.reset();
  page_factory_receiver_.Bind(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<bookmark_bar::mojom::PageHandlerFactory> receiver) {
  bookmark_bar_page_factory_receiver_.reset();
  bookmark_bar_page_factory_receiver_.Bind(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<extensions_bar::mojom::PageHandlerFactory> receiver) {
  extensions_bar_page_factory_receiver_.reset();
  extensions_bar_page_factory_receiver_.Bind(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<searchbox::mojom::PageHandler> pending_page_handler) {
  content::WebUI* webui = web_ui();
  content::WebContents* web_contents = webui->GetWebContents();
  // TODO(crbug.com/445510209): Pass `metrics_reporter_` after installing a
  // WebUIOmniboxHandler.
  realbox_handler_ = std::make_unique<RealboxHandler>(
      std::move(pending_page_handler), Profile::FromWebUI(webui), web_contents,
      base::BindRepeating(&WebUIBrowserUI::GetContextualSessionHandle,
                          base::Unretained(this)));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<guest_contents::mojom::GuestContentsHost> receiver) {
  guest_contents::GuestContentsHostImpl::Create(web_ui()->GetWebContents(),
                                                std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<tabs_api::mojom::TabStripService> receiver) {
  auto* tab_strip_service_feature =
      browser_->browser_window_features()->tab_strip_service_feature();
  CHECK(tab_strip_service_feature)
      << "Browser missing TabStripService, did you enable "
         "TabStripBrowserApi feature flag?";
  tab_strip_service_feature->Accept(std::move(receiver));
}

void WebUIBrowserUI::BindInterface(
    mojo::PendingReceiver<tracked_element::mojom::TrackedElementHandler>
        receiver) {
  const ui::ElementContext context =
      BrowserElements::From(browser_)->GetContext();
  tracked_element_handler_ = std::make_unique<ui::TrackedElementHandler>(
      web_ui()->GetWebContents(), std::move(receiver), context,
      GetKnownElementIdentifiers());
}

base::WeakPtr<WebUIBrowserUI> WebUIBrowserUI::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WebUIBrowserUI::CreatePageHandler(
    mojo::PendingRemote<webui_browser::mojom::Page> page,
    mojo::PendingReceiver<webui_browser::mojom::PageHandler> receiver) {
  page_.reset();
  page_.Bind(std::move(page));
  auto* render_frame_host = web_ui()->GetRenderFrameHost();
  WebUIBrowserPageHandler::CreateForRenderFrameHost(*render_frame_host,
                                                    std::move(receiver), this);
}

void WebUIBrowserUI::GetTabStripInset(GetTabStripInsetCallback callback) {
  std::move(callback).Run(
#if BUILDFLAG(IS_MAC)
      // Values from BrowserFrameViewMac::GetCaptionButtonBounds()
      (base::mac::MacOSVersion() >= 26'00'00) ? 76 : 82
#else
      0
#endif
  );
}

void WebUIBrowserUI::CreatePageHandler(
    mojo::PendingRemote<bookmark_bar::mojom::Page> page,
    mojo::PendingReceiver<bookmark_bar::mojom::PageHandler> receiver) {
  bookmark_bar_page_handler_ =
      std::make_unique<WebUIBrowserBookmarkBarPageHandler>(
          std::move(receiver), std::move(page), web_ui(), browser_);
}

void WebUIBrowserUI::CreatePageHandler(
    mojo::PendingRemote<extensions_bar::mojom::Page> page,
    mojo::PendingReceiver<extensions_bar::mojom::PageHandler> receiver) {
  static_cast<WebUIBrowserExtensionsContainer*>(
      browser_window()->GetExtensionsContainer())
      ->Bind(std::move(page), std::move(receiver));
}

const std::vector<ui::ElementIdentifier>&
WebUIBrowserUI::GetKnownElementIdentifiers() const {
  static const std::vector<ui::ElementIdentifier> kKnownElementIdentifiers{
      kContentsContainerViewElementId, kExtensionsMenuButtonElementId,
      kLocationBarElementId,           kLocationIconElementId,
      kToolbarAppMenuButtonElementId,  kToolbarAvatarButtonElementId};
  return kKnownElementIdentifiers;
}

void WebUIBrowserUI::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  if (bookmark_bar_page_handler_) {
    bookmark_bar_page_handler_->SetBookmarkBarState(
        BookmarkBarController::From(browser_)->bookmark_bar_state(),
        change_type);
  }
}

void WebUIBrowserUI::ShowSidePanel(SidePanelEntryKey side_panel_entry_key) {
  // Create guest contents.
  WebUIBrowserSidePanelUI* side_panel_ui =
      browser_window()->GetWebUIBrowserSidePanelUI();
  content::WebContents* web_contents =
      side_panel_ui->GetWebContentsForId(side_panel_entry_key.id());
  web_contents->SetColorProviderSource(browser_window());
  CHECK(web_contents);
  auto* guest_handle =
      guest_contents::GuestContentsHandle::CreateForWebContents(web_contents);

  // Notify JS.
  page_->ShowSidePanel(guest_handle->id(),
                       SidePanelEntryIdToTitle(side_panel_entry_key.id()));
}

void WebUIBrowserUI::CloseSidePanel() {
  page_->CloseSidePanel();
}

contextual_search::ContextualSearchSessionHandle*
WebUIBrowserUI::GetContextualSessionHandle() {
  if (!session_handle_) {
    auto* service = ContextualSearchServiceFactory::GetForProfile(
        Profile::FromWebUI(web_ui()));
    if (service) {
      // TODO(crbug.com/445510209): Use appropriate config and source
      session_handle_ = service->CreateSession(
          omnibox::CreateQueryControllerConfigParams(),
          contextual_search::ContextualSearchSource::kOmnibox);
    }
  }
  return session_handle_.get();
}

WEB_UI_CONTROLLER_TYPE_IMPL(WebUIBrowserUI)
