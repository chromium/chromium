// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/web_apps/web_app_blocked_migration_infobar_delegate.h"

#include <memory>

#include "chrome/browser/infobars/confirm_infobar_creator.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
namespace web_app {

// static
infobars::InfoBar* WebAppBlockedMigrationInfoBarDelegate::FindInfoBar(
    content::WebContents* web_contents) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  if (!infobar_manager) {
    return nullptr;
  }
  for (const auto& infobar : infobar_manager->infobars()) {
    if (infobar->delegate()->GetIdentifier() ==
        infobars::InfoBarDelegate::
            WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE) {
      return infobar.get();
    }
  }
  return nullptr;
}

// static
void WebAppBlockedMigrationInfoBarDelegate::Create(
    content::WebContents* web_contents,
    base::OnceClosure on_dismiss_callback) {
  if (FindInfoBar(web_contents)) {
    return;  // Already exists, don't show another one.
  }

  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  if (!infobar_manager) {
    return;
  }

  infobar_manager->AddInfoBar(CreateConfirmInfoBar(
      base::WrapUnique(new WebAppBlockedMigrationInfoBarDelegate(
          std::move(on_dismiss_callback)))));
}

// static
void WebAppBlockedMigrationInfoBarDelegate::Remove(
    content::WebContents* web_contents) {
  if (infobars::InfoBar* infobar = FindInfoBar(web_contents)) {
    infobars::ContentInfoBarManager* infobar_manager =
        infobars::ContentInfoBarManager::FromWebContents(web_contents);
    if (infobar_manager) {
      infobar_manager->RemoveInfoBar(infobar);
    }
  }
}

bool WebAppBlockedMigrationInfoBarDelegate::ShouldExpire(
    const NavigationDetails& details) const {
  return false;
}

// TODO(crbug.com/488031001) Find learn more link with more context.
WebAppBlockedMigrationInfoBarDelegate::WebAppBlockedMigrationInfoBarDelegate(
    base::OnceClosure on_dismiss_callback)
    : on_dismiss_callback_(std::move(on_dismiss_callback)),
      learn_more_url_("https://support.google.com/chrome") {
  CHECK(on_dismiss_callback_);
}

WebAppBlockedMigrationInfoBarDelegate::
    ~WebAppBlockedMigrationInfoBarDelegate() = default;

infobars::InfoBarDelegate::InfoBarIdentifier
WebAppBlockedMigrationInfoBarDelegate::GetIdentifier() const {
  return WEB_APP_BLOCKED_MIGRATION_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& WebAppBlockedMigrationInfoBarDelegate::GetVectorIcon()
    const {
  return vector_icons::kSettingsIcon;
}

std::u16string WebAppBlockedMigrationInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringUTF16(IDS_WEB_APP_BLOCKED_MIGRATION_MESSAGE);
}

std::u16string WebAppBlockedMigrationInfoBarDelegate::GetLinkText() const {
  return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
}

GURL WebAppBlockedMigrationInfoBarDelegate::GetLinkURL() const {
  return learn_more_url_;
}

int WebAppBlockedMigrationInfoBarDelegate::GetButtons() const {
  return BUTTON_OK;
}

bool WebAppBlockedMigrationInfoBarDelegate::Accept() {
  if (on_dismiss_callback_) {
    std::move(on_dismiss_callback_).Run();
  }
  return true;
}

void WebAppBlockedMigrationInfoBarDelegate::InfoBarDismissed() {
  if (on_dismiss_callback_) {
    std::move(on_dismiss_callback_).Run();
  }
}

}  // namespace web_app
