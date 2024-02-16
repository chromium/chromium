// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_controller.h"

#include <algorithm>
#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/language/language_model_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/read_anything/read_anything_tab_helper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_container_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_controller.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_coordinator.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_side_panel_web_view.h"
#include "chrome/browser/ui/views/side_panel/read_anything/read_anything_toolbar_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_prefs.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_page_handler.h"
#include "chrome/browser/ui/webui/side_panel/read_anything/read_anything_untrusted_ui.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "chrome/grit/generated_resources.h"
#include "components/language/core/browser/language_model.h"
#include "components/language/core/browser/language_model_manager.h"
#include "components/language/core/common/locale_util.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_types.h"

using SidePanelWebUIViewT_ReadAnythingUntrustedUI =
    SidePanelWebUIViewT<ReadAnythingUntrustedUI>;
DECLARE_TEMPLATE_METADATA(SidePanelWebUIViewT_ReadAnythingUntrustedUI,
                          SidePanelWebUIViewT);

ReadAnythingSidePanelController::ReadAnythingSidePanelController(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  // Create the model and initialize it with user prefs (if present).
  model_ = std::make_unique<ReadAnythingModel>();
  InitModelWithUserPrefs();

  // Create the controller.
  controller_ =
      std::make_unique<ReadAnythingController>(model_.get(), web_contents_);
}

void ReadAnythingSidePanelController::InitModelWithUserPrefs() {
  if (!Profile::FromBrowserContext(web_contents_->GetBrowserContext()) ||
      !Profile::FromBrowserContext(web_contents_->GetBrowserContext())
           ->GetPrefs()) {
    return;
  }

  // Get user's default language to check for compatible fonts.
  language::LanguageModel* language_model =
      LanguageModelManagerFactory::GetForBrowserContext(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext()))
          ->GetPrimaryModel();
  std::string prefs_lang = language_model->GetLanguages().front().lang_code;
  prefs_lang = language::ExtractBaseLanguage(prefs_lang);

  std::string prefs_font_name =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs()
          ->GetString(prefs::kAccessibilityReadAnythingFontName);

  double prefs_font_scale =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs()
          ->GetDouble(prefs::kAccessibilityReadAnythingFontScale);

  bool prefs_links_enabled =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext())
          ->GetPrefs()
          ->GetBoolean(prefs::kAccessibilityReadAnythingLinksEnabled);

  read_anything::mojom::Colors prefs_colors =
      static_cast<read_anything::mojom::Colors>(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->GetPrefs()
              ->GetInteger(prefs::kAccessibilityReadAnythingColorInfo));

  read_anything::mojom::LineSpacing prefs_line_spacing =
      static_cast<read_anything::mojom::LineSpacing>(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->GetPrefs()
              ->GetInteger(prefs::kAccessibilityReadAnythingLineSpacing));

  read_anything::mojom::LetterSpacing prefs_letter_spacing =
      static_cast<read_anything::mojom::LetterSpacing>(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())
              ->GetPrefs()
              ->GetInteger(prefs::kAccessibilityReadAnythingLetterSpacing));

  model_->Init(
      /* lang code = */ prefs_lang,
      /* font name = */ prefs_font_name,
      /* font scale = */ prefs_font_scale,
      /* links enabled = */ prefs_links_enabled,
      /* colors = */ prefs_colors,
      /* line spacing = */ prefs_line_spacing,
      /* letter spacing = */ prefs_letter_spacing);
  default_language_code_ = prefs_lang;
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.SetDefaultLanguageCode(prefs_lang);
  }
}

ReadAnythingSidePanelController::~ReadAnythingSidePanelController() {
  // Inform observers when |this| is destroyed so they can do their own cleanup.
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.OnSidePanelControllerDestroyed();
  }
}

void ReadAnythingSidePanelController::CreateAndRegisterEntry() {
  auto* registry = SidePanelRegistry::Get(web_contents_);
  if (!registry || registry->GetEntryForKey(SidePanelEntry::Key(
                       SidePanelEntry::Id::kReadAnything))) {
    return;
  }

  auto side_panel_entry = std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kReadAnything,
      l10n_util::GetStringUTF16(IDS_READING_MODE_TITLE),
      ui::ImageModel::FromVectorIcon(kMenuBookChromeRefreshIcon,
                                     ui::kColorIcon),
      base::BindRepeating(&ReadAnythingSidePanelController::CreateContainerView,
                          base::Unretained(this)));
  side_panel_entry->AddObserver(this);
  registry->Register(std::move(side_panel_entry));
}

void ReadAnythingSidePanelController::DeregisterEntry() {
  auto* registry = SidePanelRegistry::Get(web_contents_);
  if (!registry) {
    return;
  }

  if (auto* current_entry = registry->GetEntryForKey(
          SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything))) {
    current_entry->RemoveObserver(this);
  }
  registry->Deregister(SidePanelEntry::Key(SidePanelEntry::Id::kReadAnything));
}

void ReadAnythingSidePanelController::AddPageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  AddObserver(page_handler.get());
  AddModelObserver(page_handler.get());
}

void ReadAnythingSidePanelController::RemovePageHandlerAsObserver(
    base::WeakPtr<ReadAnythingUntrustedPageHandler> page_handler) {
  RemoveObserver(page_handler.get());
  RemoveModelObserver(page_handler.get());
}

void ReadAnythingSidePanelController::AddObserver(
    ReadAnythingSidePanelController::Observer* observer) {
  observers_.AddObserver(observer);

  // InitModelWithUserPrefs where default_language_code_ is set may be called
  // before all observerers have been added, so ensure that observers are
  // updated with the correct language code as they're added.
  observer->SetDefaultLanguageCode(default_language_code_);
}

void ReadAnythingSidePanelController::RemoveObserver(
    ReadAnythingSidePanelController::Observer* observer) {
  observers_.RemoveObserver(observer);
}

void ReadAnythingSidePanelController::AddModelObserver(
    ReadAnythingModel::Observer* observer) {
  DCHECK(model_);
  model_->AddObserver(observer);
}

void ReadAnythingSidePanelController::RemoveModelObserver(
    ReadAnythingModel::Observer* observer) {
  DCHECK(model_);
  model_->RemoveObserver(observer);
}

void ReadAnythingSidePanelController::OnEntryShown(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents_)) {
    auto* coordinator = ReadAnythingCoordinator::GetOrCreateForBrowser(browser);
    coordinator->OnReadAnythingSidePanelEntryShown();
  }
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.Activate(true);
  }
}

void ReadAnythingSidePanelController::OnEntryHidden(SidePanelEntry* entry) {
  CHECK_EQ(entry->key().id(), SidePanelEntry::Id::kReadAnything);
  if (Browser* browser = chrome::FindBrowserWithTab(web_contents_)) {
    auto* coordinator = ReadAnythingCoordinator::GetOrCreateForBrowser(browser);
    coordinator->OnReadAnythingSidePanelEntryHidden();
  }
  for (ReadAnythingSidePanelController::Observer& obs : observers_) {
    obs.Activate(false);
  }
}

std::unique_ptr<views::View>
ReadAnythingSidePanelController::CreateContainerView() {
  auto web_view = std::make_unique<ReadAnythingSidePanelWebView>(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));

  if (features::IsReadAnythingWebUIToolbarEnabled()) {
    return std::move(web_view);
  }

  // Create the views.
  auto toolbar = std::make_unique<ReadAnythingToolbarView>(
      this,
      /*toolbar_delegate=*/controller_.get(),
      /*font_combobox_delegate=*/controller_.get());

  // Create the component.
  // Note that a side panel controller would normally maintain ownership of
  // these objects, but objects extending {ui/views/view.h} prefer ownership
  // over raw pointers (View ownership is typically managed by the View
  // hierarchy, rather than by outside controllers).
  auto container_view = std::make_unique<ReadAnythingContainerView>(
      this, std::move(toolbar), std::move(web_view));

  return std::move(container_view);
}
