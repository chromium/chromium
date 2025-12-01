// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar_handler.h"

#include "base/feature_list.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom-data-view.h"
#include "chrome/browser/ui/webui/side_panel/customize_chrome/customize_toolbar/customize_toolbar.mojom.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/contextual_tasks/public/features.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
#include "ui/display/screen.h"

namespace {
std::optional<side_panel::customize_chrome::mojom::ActionId>
MojoActionForChromeAction(actions::ActionId action_id) {
  switch (action_id) {
    case kActionSidePanelShowBookmarks:
      return side_panel::customize_chrome::mojom::ActionId::kShowBookmarks;
    case kActionSidePanelShowHistoryCluster:
      return side_panel::customize_chrome::mojom::ActionId::kShowHistoryCluster;
    case kActionSidePanelShowReadAnything:
      return side_panel::customize_chrome::mojom::ActionId::kShowReadAnything;
    case kActionSidePanelShowReadingList:
      return side_panel::customize_chrome::mojom::ActionId::kShowReadingList;
    case kActionSidePanelShowLensOverlayResults:
      return side_panel::customize_chrome::mojom::ActionId::kShowLensOverlay;
    case kActionSidePanelShowSearchCompanion:
      return side_panel::customize_chrome::mojom::ActionId::
          kShowSearchCompanion;
    case kActionHome:
      return side_panel::customize_chrome::mojom::ActionId::kHome;
    case kActionForward:
      return side_panel::customize_chrome::mojom::ActionId::kForward;
    case kActionNewIncognitoWindow:
      return side_panel::customize_chrome::mojom::ActionId::kNewIncognitoWindow;
    case kActionShowPasswordsBubbleOrPage:
      return side_panel::customize_chrome::mojom::ActionId::
          kShowPasswordManager;
    case kActionShowPaymentsBubbleOrPage:
      return side_panel::customize_chrome::mojom::ActionId::kShowPaymentMethods;
    case kActionShowAddressesBubbleOrPage:
      return side_panel::customize_chrome::mojom::ActionId::kShowAddresses;
    case kActionShowDownloads:
      return side_panel::customize_chrome::mojom::ActionId::kShowDownloads;
    case kActionClearBrowsingData:
      return side_panel::customize_chrome::mojom::ActionId::kClearBrowsingData;
    case kActionPrint:
      return side_panel::customize_chrome::mojom::ActionId::kPrint;
    case kActionShowTranslate:
      return side_panel::customize_chrome::mojom::ActionId::kShowTranslate;
    case kActionSendTabToSelf:
      return side_panel::customize_chrome::mojom::ActionId::kSendTabToSelf;
    case kActionQrCodeGenerator:
      return side_panel::customize_chrome::mojom::ActionId::kQrCodeGenerator;
    case kActionRouteMedia:
      return side_panel::customize_chrome::mojom::ActionId::kRouteMedia;
    case kActionTaskManager:
      return side_panel::customize_chrome::mojom::ActionId::kTaskManager;
    case kActionDevTools:
      return side_panel::customize_chrome::mojom::ActionId::kDevTools;
    case kActionShowChromeLabs:
      return side_panel::customize_chrome::mojom::ActionId::kShowChromeLabs;
    case kActionCopyUrl:
      return side_panel::customize_chrome::mojom::ActionId::kCopyLink;
    case kActionTabSearch:
      return side_panel::customize_chrome::mojom::ActionId::kTabSearch;
    case kActionSplitTab:
      return side_panel::customize_chrome::mojom::ActionId::kSplitTab;
    case kActionSidePanelShowContextualTasks:
      return side_panel::customize_chrome::mojom::ActionId::kContextualTasks;
    default:
      return std::nullopt;
  }
}

std::optional<actions::ActionId> ChromeActionForMojoAction(
    side_panel::customize_chrome::mojom::ActionId action_id) {
  switch (action_id) {
    case side_panel::customize_chrome::mojom::ActionId::kShowBookmarks:
      return kActionSidePanelShowBookmarks;
    case side_panel::customize_chrome::mojom::ActionId::kShowHistoryCluster:
      return kActionSidePanelShowHistoryCluster;
    case side_panel::customize_chrome::mojom::ActionId::kShowReadAnything:
      return kActionSidePanelShowReadAnything;
    case side_panel::customize_chrome::mojom::ActionId::kShowReadingList:
      return kActionSidePanelShowReadingList;
    case side_panel::customize_chrome::mojom::ActionId::kShowLensOverlay:
      return kActionSidePanelShowLensOverlayResults;
    case side_panel::customize_chrome::mojom::ActionId::kShowSearchCompanion:
      return kActionSidePanelShowSearchCompanion;
    case side_panel::customize_chrome::mojom::ActionId::kHome:
      return kActionHome;
    case side_panel::customize_chrome::mojom::ActionId::kForward:
      return kActionForward;
    case side_panel::customize_chrome::mojom::ActionId::kNewIncognitoWindow:
      return kActionNewIncognitoWindow;
    case side_panel::customize_chrome::mojom::ActionId::kShowPasswordManager:
      return kActionShowPasswordsBubbleOrPage;
    case side_panel::customize_chrome::mojom::ActionId::kShowPaymentMethods:
      return kActionShowPaymentsBubbleOrPage;
    case side_panel::customize_chrome::mojom::ActionId::kShowAddresses:
      return kActionShowAddressesBubbleOrPage;
    case side_panel::customize_chrome::mojom::ActionId::kShowDownloads:
      return kActionShowDownloads;
    case side_panel::customize_chrome::mojom::ActionId::kClearBrowsingData:
      return kActionClearBrowsingData;
    case side_panel::customize_chrome::mojom::ActionId::kPrint:
      return kActionPrint;
    case side_panel::customize_chrome::mojom::ActionId::kShowTranslate:
      return kActionShowTranslate;
    case side_panel::customize_chrome::mojom::ActionId::kSendTabToSelf:
      return kActionSendTabToSelf;
    case side_panel::customize_chrome::mojom::ActionId::kQrCodeGenerator:
      return kActionQrCodeGenerator;
    case side_panel::customize_chrome::mojom::ActionId::kRouteMedia:
      return kActionRouteMedia;
    case side_panel::customize_chrome::mojom::ActionId::kTaskManager:
      return kActionTaskManager;
    case side_panel::customize_chrome::mojom::ActionId::kDevTools:
      return kActionDevTools;
    case side_panel::customize_chrome::mojom::ActionId::kShowChromeLabs:
      return kActionShowChromeLabs;
    case side_panel::customize_chrome::mojom::ActionId::kCopyLink:
      return kActionCopyUrl;
    case side_panel::customize_chrome::mojom::ActionId::kTabSearch:
      return kActionTabSearch;
    case side_panel::customize_chrome::mojom::ActionId::kSplitTab:
      return kActionSplitTab;
    case side_panel::customize_chrome::mojom::ActionId::kContextualTasks:
      return kActionSidePanelShowContextualTasks;
    default:
      return std::nullopt;
  }
}
}  // namespace

CustomizeToolbarHandler::CustomizeToolbarHandler(
    mojo::PendingReceiver<
        side_panel::customize_chrome::mojom::CustomizeToolbarHandler> handler,
    mojo::PendingRemote<
        side_panel::customize_chrome::mojom::CustomizeToolbarClient> client,
    content::WebContents* web_contents)
    : client_(std::move(client)),
      receiver_(this, std::move(handler)),
      web_contents_(web_contents),
      model_(PinnedToolbarActionsModel::Get(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))) {
  model_observation_.Observe(model_);
  pref_change_registrar_.Init(prefs());

  pref_change_registrar_.Add(
      prefs::kShowHomeButton,
      base::BindRepeating(&CustomizeToolbarHandler::OnActionPinnedChanged,
                          base::Unretained(this), kActionHome,
                          prefs::kShowHomeButton));
  pref_change_registrar_.Add(
      prefs::kShowForwardButton,
      base::BindRepeating(&CustomizeToolbarHandler::OnActionPinnedChanged,
                          base::Unretained(this), kActionForward,
                          prefs::kShowForwardButton));
  pref_change_registrar_.Add(
      prefs::kPinContextualTaskButton,
      base::BindRepeating(&CustomizeToolbarHandler::OnActionPinnedChanged,
                          base::Unretained(this),
                          kActionSidePanelShowContextualTasks,
                          prefs::kPinContextualTaskButton));
  pref_change_registrar_.Add(
      prefs::kPinSplitTabButton,
      base::BindRepeating(&CustomizeToolbarHandler::OnActionPinnedChanged,
                          base::Unretained(this), kActionSplitTab,
                          prefs::kPinSplitTabButton));
}

CustomizeToolbarHandler::~CustomizeToolbarHandler() = default;

void CustomizeToolbarHandler::ListActions(ListActionsCallback callback) {
  std::vector<side_panel::customize_chrome::mojom::ActionPtr> actions;
  const raw_ptr<BrowserWindowInterface> bwi =
      webui::GetBrowserWindowInterface(web_contents_);
  if (!bwi) {
    std::move(callback).Run(std::move(actions));
    return;
  }

  const ui::ColorProvider& provider = web_contents_->GetColorProvider();
  const int icon_color_id = ui::kColorSysOnSurface;
  const float scale_factor =
      display::Screen::Get()
          ->GetDisplayNearestWindow(web_contents_->GetTopLevelNativeWindow())
          .device_scale_factor();

  auto home_action = side_panel::customize_chrome::mojom::Action::New(
      MojoActionForChromeAction(kActionHome).value(),
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_ACCNAME_HOME)),
      prefs()->GetBoolean(prefs::kShowHomeButton), false,
      side_panel::customize_chrome::mojom::CategoryId::kNavigation,
      GURL(webui::EncodePNGAndMakeDataURI(
          ui::ImageModel::FromVectorIcon(kNavigateHomeChromeRefreshIcon,
                                         icon_color_id)
              .Rasterize(&provider),
          scale_factor)));

  auto forward_action = side_panel::customize_chrome::mojom::Action::New(
      MojoActionForChromeAction(kActionForward).value(),
      base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_ACCNAME_FORWARD)),
      prefs()->GetBoolean(prefs::kShowForwardButton), false,
      side_panel::customize_chrome::mojom::CategoryId::kNavigation,
      GURL(webui::EncodePNGAndMakeDataURI(
          ui::ImageModel::FromVectorIcon(
              vector_icons::kForwardArrowChromeRefreshIcon, icon_color_id)
              .Rasterize(&provider),
          scale_factor)));

  actions.push_back(std::move(home_action));
  actions.push_back(std::move(forward_action));

  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    auto split_tab_action = side_panel::customize_chrome::mojom::Action::New(
        MojoActionForChromeAction(kActionSplitTab).value(),
        base::UTF16ToUTF8(l10n_util::GetStringUTF16(IDS_PIN_SPLIT_TAB_TOGGLE)),
        prefs()->GetBoolean(prefs::kPinSplitTabButton), false,
        side_panel::customize_chrome::mojom::CategoryId::kNavigation,
        GURL(webui::EncodePNGAndMakeDataURI(
            ui::ImageModel::FromVectorIcon(kSplitSceneIcon, icon_color_id)
                .Rasterize(&provider),
            scale_factor)));

    actions.push_back(std::move(split_tab_action));
  }

  if (base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks) &&
      (contextual_tasks::kShowEntryPoint.Get() ==
       contextual_tasks::EntryPointOption::kToolbarPermanent)) {
    auto contextual_task_action =
        side_panel::customize_chrome::mojom::Action::New(
            MojoActionForChromeAction(kActionSidePanelShowContextualTasks)
                .value(),
            base::UTF16ToUTF8(l10n_util::GetStringUTF16(
                IDS_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_TITLE)),
            prefs()->GetBoolean(prefs::kPinContextualTaskButton), false,
            side_panel::customize_chrome::mojom::CategoryId::kNavigation,
            GURL(webui::EncodePNGAndMakeDataURI(
                ui::ImageModel::FromVectorIcon(kDockToRightSparkIcon,
                                               icon_color_id)
                    .Rasterize(&provider),
                scale_factor)));
    actions.push_back(std::move(contextual_task_action));
  }

  const auto add_action =
      [&actions, this, &provider, scale_factor, bwi](
          actions::ActionId id,
          side_panel::customize_chrome::mojom::CategoryId category) {
        actions::ActionItem* const scope_action =
            bwi->GetActions()->root_action_item();
        actions::ActionItem* const action_item =
            actions::ActionManager::Get().FindAction(id, scope_action);
        if (!action_item || !action_item->GetVisible()) {
          return;
        }

        if (!action_observations_.contains(id)) {
          action_observations_.emplace(
              id, action_item->AddActionChangedCallback(base::BindRepeating(
                      &CustomizeToolbarHandler::OnActionItemChanged,
                      base::Unretained(this))));
        }

        switch (static_cast<actions::ActionPinnableState>(
            action_item->GetProperty(actions::kActionItemPinnableKey))) {
          case actions::ActionPinnableState::kNotPinnable:
            return;
          case actions::ActionPinnableState::kPinnable:
          case actions::ActionPinnableState::kEnterpriseControlled:
            break;
          default:
            NOTREACHED();
        }

        // If the icon is a vector icon, recolor it to match the spec.
        // Non-vector icons cannot be recolored, but there aren't any of those
        // currently anyways.
        const ui::ImageModel& original_icon = action_item->GetImage();
        const ui::ImageModel recolored_icon =
            original_icon.IsVectorIcon()
                ? ui::ImageModel::FromVectorIcon(
                      *(action_item->GetImage().GetVectorIcon().vector_icon()),
                      icon_color_id,
                      action_item->GetImage().GetVectorIcon().icon_size())
                : original_icon;

        bool has_enterprise_controlled_pinned_state =
            action_item->GetProperty(actions::kActionItemPinnableKey) ==
            std::underlying_type_t<actions::ActionPinnableState>(
                actions::ActionPinnableState::kEnterpriseControlled);
        auto mojo_action = side_panel::customize_chrome::mojom::Action::New(
            MojoActionForChromeAction(id).value(),
            base::UTF16ToUTF8(action_item->GetText()), model_->Contains(id),
            has_enterprise_controlled_pinned_state, category,
            GURL(webui::EncodePNGAndMakeDataURI(
                recolored_icon.Rasterize(&provider), scale_factor)));
        actions.push_back(std::move(mojo_action));
      };

  add_action(kActionNewIncognitoWindow,
             side_panel::customize_chrome::mojom::CategoryId::kNavigation);

  add_action(kActionShowPasswordsBubbleOrPage,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  add_action(kActionShowPaymentsBubbleOrPage,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  add_action(kActionShowAddressesBubbleOrPage,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  add_action(kActionSidePanelShowBookmarks,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  add_action(kActionSidePanelShowReadingList,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  add_action(kActionSidePanelShowHistoryCluster,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  add_action(kActionShowDownloads,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);
  add_action(kActionClearBrowsingData,
             side_panel::customize_chrome::mojom::CategoryId::kYourChrome);

  if (features::HasTabSearchToolbarButton()) {
    add_action(kActionTabSearch,
               side_panel::customize_chrome::mojom::CategoryId::kTools);
  }
  add_action(kActionPrint,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionSidePanelShowLensOverlayResults,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionSidePanelShowSearchCompanion,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionShowTranslate,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionQrCodeGenerator,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionRouteMedia,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionSidePanelShowReadAnything,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionCopyUrl,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionSendTabToSelf,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionTaskManager,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionDevTools,
             side_panel::customize_chrome::mojom::CategoryId::kTools);
  add_action(kActionShowChromeLabs,
             side_panel::customize_chrome::mojom::CategoryId::kTools);

  std::move(callback).Run(std::move(actions));
}

void CustomizeToolbarHandler::ListCategories(ListCategoriesCallback callback) {
  std::vector<side_panel::customize_chrome::mojom::CategoryPtr> categories;

  categories.push_back(side_panel::customize_chrome::mojom::Category::New(
      side_panel::customize_chrome::mojom::CategoryId::kNavigation,
      l10n_util::GetStringUTF8(IDS_NTP_CUSTOMIZE_TOOLBAR_CATEGORY_NAVIGATION)));
  categories.push_back(side_panel::customize_chrome::mojom::Category::New(
      side_panel::customize_chrome::mojom::CategoryId::kYourChrome,
      l10n_util::GetStringUTF8(
          IDS_NTP_CUSTOMIZE_TOOLBAR_CATEGORY_YOUR_CHROME)));
  categories.push_back(side_panel::customize_chrome::mojom::Category::New(
      side_panel::customize_chrome::mojom::CategoryId::kTools,
      l10n_util::GetStringUTF8(
          IDS_NTP_CUSTOMIZE_TOOLBAR_CATEGORY_TOOLS_AND_ACTIONS)));

  std::move(callback).Run(std::move(categories));
}

void CustomizeToolbarHandler::PinAction(
    side_panel::customize_chrome::mojom::ActionId action_id,
    bool pin) {
  const std::optional<actions::ActionId> chrome_action =
      ChromeActionForMojoAction(action_id);
  if (!chrome_action.has_value()) {
    mojo::ReportBadMessage("PinAction called with an unsupported action.");
    return;
  }

  switch (chrome_action.value()) {
    case kActionHome:
      prefs()->SetBoolean(prefs::kShowHomeButton, pin);
      break;
    case kActionForward:
      prefs()->SetBoolean(prefs::kShowForwardButton, pin);
      break;
    case kActionSidePanelShowContextualTasks:
      prefs()->SetBoolean(prefs::kPinContextualTaskButton, pin);
      break;
    case kActionSplitTab:
      prefs()->SetBoolean(prefs::kPinSplitTabButton, pin);
      break;
    default:
      model_->UpdatePinnedState(chrome_action.value(), pin);
      const std::optional<std::string> metrics_name =
          actions::ActionIdMap::ActionIdToString(chrome_action.value());
      CHECK(metrics_name.has_value());
      base::RecordComputedAction(base::StrCat(
          {"Actions.PinnedToolbarButton.", pin ? "Pinned" : "Unpinned",
           ".ByCustomizeChromeSidePanel.", metrics_name.value()}));
      base::RecordComputedAction(base::StrCat({"Actions.PinnedToolbarButton.",
                                               pin ? "Pinned" : "Unpinned",
                                               ".ByCustomizeChromeSidePanel"}));
  }
}

void CustomizeToolbarHandler::GetIsCustomized(
    GetIsCustomizedCallback callback) {
  std::move(callback).Run(!model_->IsDefault());
}

void CustomizeToolbarHandler::ResetToDefault() {
  model_->ResetToDefault();
}

void CustomizeToolbarHandler::OnActionsChanged() {
  client_->NotifyActionsUpdated();
}

void CustomizeToolbarHandler::OnActionPinnedChanged(actions::ActionId id,
                                                    std::string_view pref) {
  const std::optional<side_panel::customize_chrome::mojom::ActionId>
      mojo_action_id = MojoActionForChromeAction(id);
  if (!mojo_action_id.has_value()) {
    return;
  }

  const bool pinned = prefs()->GetBoolean(pref);
  client_->SetActionPinned(mojo_action_id.value(), pinned);
}

void CustomizeToolbarHandler::OnActionItemChanged() {
  client_->NotifyActionsUpdated();
}

PrefService* CustomizeToolbarHandler::prefs() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->GetPrefs();
}
