// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/webapps/browser/android/installable/installable_ambient_badge_message_controller.h"

#include "base/command_line.h"
#include "base/containers/lru_cache.h"
#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "components/messages/android/message_dispatcher_bridge.h"
#include "components/messages/android/throttler/domain_session_throttler.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/elide_url.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_client.h"
#include "components/webapps/browser/android/webapps_icon_utils.h"
#include "components/webapps/browser/features.h"
#include "components/webapps/common/switches.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

namespace webapps {

namespace {
constexpr int kThrottleDomainsCapacity = 100;
}

InstallableAmbientBadgeMessageController::
    InstallableAmbientBadgeMessageController(
        InstallableAmbientBadgeClient* client)
    : client_(client) {}

InstallableAmbientBadgeMessageController::
    ~InstallableAmbientBadgeMessageController() {
  DismissMessage();
}

bool InstallableAmbientBadgeMessageController::IsMessageEnqueued() {
  return message_ != nullptr;
}

void InstallableAmbientBadgeMessageController::EnqueueMessage(
    content::WebContents* web_contents,
    const std::u16string& app_name,
    const SkBitmap& icon,
    const bool is_primary_icon_maskable,
    const GURL& start_url) {
  DCHECK(!message_);
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kBypassInstallThrottleForTesting) &&
      !GetThrottler()->ShouldShow(
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin())) {
    return;
  }

  save_origin_ = web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin();
  message_ = std::make_unique<messages::MessageWrapper>(
      messages::MessageIdentifier::INSTALLABLE_AMBIENT_BADGE,
      base::BindOnce(
          &InstallableAmbientBadgeMessageController::HandleInstallButtonClicked,
          weak_factory_.GetWeakPtr()),
      base::BindOnce(
          &InstallableAmbientBadgeMessageController::HandleMessageDismissed,
          weak_factory_.GetWeakPtr()));

  message_->SetTitle(l10n_util::GetStringFUTF16(
      IDS_AMBIENT_BADGE_INSTALL_ALTERNATIVE, app_name));
  message_->SetDescription(url_formatter::FormatUrlForSecurityDisplay(
      start_url, url_formatter::SchemeDisplay::OMIT_CRYPTOGRAPHIC));
  message_->DisableIconTint();
  if (is_primary_icon_maskable &&
      WebappsIconUtils::DoesAndroidSupportMaskableIcons()) {
    message_->SetIcon(WebappsIconUtils::GenerateAdaptiveIconBitmap(icon));
  } else {
    message_->SetIcon(icon);
  }
  message_->EnableLargeIcon(true);
  message_->SetIconRoundedCornerRadius(
      WebappsIconUtils::GetIdealIconCornerRadiusPxForPromptUI());
  message_->SetPrimaryButtonText(l10n_util::GetStringUTF16(IDS_INSTALL));
  messages::MessageDispatcherBridge::Get()->EnqueueMessage(
      message_.get(), web_contents, messages::MessageScopeType::NAVIGATION,
      messages::MessagePriority::kNormal);
}

void InstallableAmbientBadgeMessageController::DismissMessage() {
  if (!message_)
    return;

  messages::MessageDispatcherBridge::Get()->DismissMessage(
      message_.get(), messages::DismissReason::UNKNOWN);
}

void InstallableAmbientBadgeMessageController::HandleInstallButtonClicked() {
  client_->AddToHomescreenFromBadge();
}

void InstallableAmbientBadgeMessageController::HandleMessageDismissed(
    messages::DismissReason dismiss_reason) {
  DCHECK(message_);
  message_.reset();
  if (dismiss_reason == messages::DismissReason::GESTURE) {
    client_->BadgeDismissed();
  } else if (dismiss_reason == messages::DismissReason::TIMER) {
    client_->BadgeIgnored();
  }

  if (dismiss_reason != messages::DismissReason::PRIMARY_ACTION) {
    GetThrottler()->AddStrike(save_origin_);
  }
}

// static
messages::DomainSessionThrottler*
InstallableAmbientBadgeMessageController::GetThrottler() {
  static messages::DomainSessionThrottler instance(kThrottleDomainsCapacity);
  return &instance;
}

}  // namespace webapps
