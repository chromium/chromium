// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/js_optimization/js_optimizations_infobar_delegate.h"

#include <memory>

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/navigation_controller.h"
#include "ui/base/l10n/l10n_util.h"

// static
void JsOptimizationsInfoBarDelegate::Create(
    infobars::ContentInfoBarManager* infobar_manager) {
  auto delegate = std::make_unique<JsOptimizationsInfoBarDelegate>();
  infobar_manager->AddInfoBar(CreateConfirmInfoBar(std::move(delegate)));
}

JsOptimizationsInfoBarDelegate::JsOptimizationsInfoBarDelegate() = default;

JsOptimizationsInfoBarDelegate::~JsOptimizationsInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
JsOptimizationsInfoBarDelegate::GetIdentifier() const {
  return JS_OPTIMIZATIONS_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& JsOptimizationsInfoBarDelegate::GetVectorIcon() const {
  return vector_icons::kSettingsChromeRefreshIcon;
}

std::u16string JsOptimizationsInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_JS_OPTIMIZATION_SITE_RELOAD_TEXT);
}

int JsOptimizationsInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

std::u16string JsOptimizationsInfoBarDelegate::GetButtonLabel(
    InfoBarButton button) const {
  DCHECK_EQ(BUTTON_OK, button);
  return l10n_util::GetStringUTF16(IDS_JS_OPTIMIZATION_SITE_RELOAD_BUTTON);
}

bool JsOptimizationsInfoBarDelegate::Accept() {
  content::WebContents* web_contents =
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar());
  web_contents->GetController().Reload(content::ReloadType::NORMAL, true);
  return true;
}
