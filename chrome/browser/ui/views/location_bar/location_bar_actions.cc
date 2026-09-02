// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_actions.h"

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/omnibox/clipboard_utils.h"
#include "chrome/browser/ui/omnibox/omnibox_controller.h"
#include "chrome/browser/ui/omnibox/omnibox_edit_model.h"
#include "chrome/browser/ui/views/location_bar/omnibox_popup_file_selector.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_presenter_delegate.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_webui_base_content.h"
#include "chrome/browser/ui/webui/cr_components/composebox/composebox_handler.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/omnibox_popup/omnibox_popup_ui.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/omnibox_popup_resources.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/menus/simple_menu_model.h"

namespace content {

class WebContents;

}  // namespace content

namespace {

LocationBar* GetLocationBarForActions(BrowserWindowInterface* browser) {
  BrowserWindow* browser_window = BrowserWindow::FromBrowser(browser);
  return browser_window ? browser_window->GetLocationBar() : nullptr;
}

OmniboxPopupUI* GetAIMPopup(OmniboxPopupPresenterDelegate* delegate) {
  if (!delegate) {
    return nullptr;
  }

  auto* aim_presenter = delegate->GetOmniboxPopupAimPresenter();
  if (!aim_presenter || !aim_presenter->GetWebUIContent()) {
    return nullptr;
  }

  auto* aim_webcontents =
      aim_presenter->GetWebUIContent()->GetWrappedWebContents();
  if (!aim_webcontents) {
    return nullptr;
  }

  if (content::WebUI* web_ui = aim_webcontents->GetWebUI()) {
    return web_ui->GetController()->GetAs<OmniboxPopupUI>();
  }
  return nullptr;
}

void AddFileOrImageToOmnibox(
    BrowserWindowInterface* browser,
    bool is_image,
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  LocationBar* const location_bar = GetLocationBarForActions(browser);
  if (!location_bar) {
    return;
  }
  OmniboxPopupPresenterDelegate* presenter_delegate =
      location_bar->GetPresenterDelegate();
  if (!presenter_delegate) {
    return;
  }

  content::WebContents* const web_contents = location_bar->GetWebContents();
  OmniboxController* const controller = location_bar->GetOmniboxController();
  OmniboxEditModel* const edit_model =
      controller ? controller->edit_model() : nullptr;
  if (!web_contents || !edit_model) {
    return;
  }
  const bool is_aim_popup_open =
      controller->popup_state_manager()->popup_state() ==
      OmniboxPopupState::kAim;
  OmniboxPopupFileSelector* const file_selector =
      presenter_delegate->GetOmniboxPopupFileSelector();
  if (file_selector) {
    file_selector->OpenFileUploadDialog(
        web_contents, is_image, edit_model,
        OmniboxPopupFileSelector::CreateImageEncodingOptions(),
        /*was_ai_mode_open=*/is_aim_popup_open);
  }
}

void SetOmniboxToolModeAndOpenAi(
    BrowserWindowInterface* browser,
    omnibox::ToolMode tool_mode,
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  LocationBar* const location_bar = GetLocationBarForActions(browser);
  if (!location_bar) {
    return;
  }
  OmniboxController* const controller = location_bar->GetOmniboxController();
  OmniboxEditModel* const edit_model =
      controller ? controller->edit_model() : nullptr;
  if (!edit_model) {
    return;
  }
  OmniboxPopupUI* const omnibox_popup_ui =
      GetAIMPopup(location_bar->GetPresenterDelegate());
  ContextualSearchboxHandler* const composebox_handler =
      omnibox_popup_ui ? omnibox_popup_ui->composebox_handler() : nullptr;
  if (composebox_handler) {
    composebox_handler->SetActiveToolMode(tool_mode,
                                          /*is_set_by_aim=*/false);
    composebox_handler->RecordToolSelectionAction(tool_mode);
  }
  edit_model->OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
}

void SetOmniboxModelModeAndOpenAi(
    BrowserWindowInterface* browser,
    omnibox::ModelMode model_mode,
    actions::ActionItem* item,
    actions::ActionInvocationContext context) {
  LocationBar* const location_bar = GetLocationBarForActions(browser);
  if (!location_bar) {
    return;
  }
  OmniboxController* const controller = location_bar->GetOmniboxController();
  OmniboxEditModel* const edit_model =
      controller ? controller->edit_model() : nullptr;
  if (!edit_model) {
    return;
  }
  OmniboxPopupUI* const omnibox_popup_ui =
      GetAIMPopup(location_bar->GetPresenterDelegate());
  ContextualSearchboxHandler* const composebox_handler =
      omnibox_popup_ui ? omnibox_popup_ui->composebox_handler() : nullptr;
  if (composebox_handler) {
    composebox_handler->SetActiveModelMode(model_mode,
                                           /*is_set_by_aim=*/false);
    composebox_handler->RecordModelSelectionAction(model_mode);
  }
  edit_model->OpenAiMode(OmniboxEditModel::AimActivation::kContextMenu);
}

void ExecutePasteAndGo(BrowserWindowInterface* browser,
                       actions::ActionItem* item,
                       actions::ActionInvocationContext context) {
  LocationBar* const location_bar = GetLocationBarForActions(browser);
  if (!location_bar || !location_bar->GetOmniboxView()) {
    return;
  }
  GetClipboardText(
      /*notify_if_restricted=*/true,
      base::BindOnce(
          [](base::WeakPtr<BrowserWindowInterface> bwi, std::u16string text) {
            if (!bwi) {
              return;
            }
            LocationBar* location_bar = GetLocationBarForActions(bwi.get());
            if (location_bar && location_bar->GetOmniboxView()) {
              if (auto* controller = location_bar->GetOmniboxController()) {
                controller->edit_model()->PasteAndGo(text);
              }
            }
          },
          browser->GetWeakPtr()));
}

}  // namespace

void RegisterOmniboxActions(
    BrowserWindowInterface* browser) {
  if (!browser) {
    return;
  }

  auto* browser_actions = BrowserActions::From(browser);
  if (!browser_actions) {
    return;
  }

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&AddFileOrImageToOmnibox,
                              base::Unretained(browser),
                              /*is_image=*/true))
          .SetText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_IMAGE))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_IMAGE))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kAddPhotoAlternateIcon
                                                : kAddPhotoAlternateOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetActionId(kActionOmniboxContextAddImage)
          .Build());

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&AddFileOrImageToOmnibox,
                              base::Unretained(browser),
                              /*is_image=*/false))
          .SetText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_FILE))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_ADD_FILE))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kAttachFileIcon
                                                : kAttachFileOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetActionId(kActionOmniboxContextAddFile)
          .Build());

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&SetOmniboxToolModeAndOpenAi,
                              base::Unretained(browser),
                              omnibox::ToolMode::TOOL_MODE_IMAGE_GEN))
          .SetText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_CREATE_IMAGES))
          .SetTooltipText(
              l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_CREATE_IMAGES))
          .SetImage(ui::ImageModel::FromResourceId(
              IDR_OMNIBOX_POPUP_IMAGES_CREATE_IMAGES_PNG))
          .SetActionId(kActionOmniboxContextCreateImages)
          .Build());

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&SetOmniboxToolModeAndOpenAi,
                              base::Unretained(browser),
                              omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH))
          .SetText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_DEEP_SEARCH))
          .SetTooltipText(
              l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_DEEP_SEARCH))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kTravelExploreIcon
                                                : kTravelExploreOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetActionId(kActionOmniboxContextDeepResearch)
          .Build());

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&SetOmniboxToolModeAndOpenAi,
                              base::Unretained(browser),
                              omnibox::ToolMode::TOOL_MODE_CANVAS))
          .SetText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_CANVAS))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_CANVAS))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kDraftSparkIcon
                                                : kDraftSparkOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetActionId(kActionOmniboxContextCanvas)
          .Build());

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(
              &SetOmniboxModelModeAndOpenAi, base::Unretained(browser),
              omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE))
          .SetText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_AUTO_MODEL))
          .SetTooltipText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_AUTO_MODEL))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kAutorenewIcon
                                                : kAutorenewOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetActionId(kActionOmniboxContextSetModelAuto)
          .Build());

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&SetOmniboxModelModeAndOpenAi,
                              base::Unretained(browser),
                              omnibox::ModelMode::MODEL_MODE_GEMINI_PRO))
          .SetText(l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_THINKING_3_PRO))
          .SetTooltipText(
              l10n_util::GetStringUTF16(IDS_NTP_COMPOSE_THINKING_3_PRO))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kTimerIcon : kTimerOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetActionId(kActionOmniboxContextSetModelThinking)
          .Build());

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&SetOmniboxModelModeAndOpenAi,
                              base::Unretained(browser),
                              omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR))
          .SetImage(ui::ImageModel::FromVectorIcon(
              features::IsRoundedIconsEnabled() ? kBoltIcon : kBoltOldIcon,
              ui::kColorIcon, ui::SimpleMenuModel::kDefaultIconSize))
          .SetActionId(kActionOmniboxContextSetModelRegular)
          .Build());

  browser_actions->RegisterAction(
      actions::ActionItem::Builder(
          base::BindRepeating(&ExecutePasteAndGo, base::Unretained(browser)))
          .SetActionId(kActionPasteAndGo)
          .Build());
}
