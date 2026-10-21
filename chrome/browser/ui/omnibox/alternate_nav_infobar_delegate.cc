// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"

#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/omnibox/chrome_omnibox_navigation_observer_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_features.h"
#include "ui/base/window_open_disposition.h"

AlternateNavInfoBarDelegate::~AlternateNavInfoBarDelegate() = default;

// static
void AlternateNavInfoBarDelegate::CreateForOmniboxNavigation(
    content::WebContents* web_contents,
    const std::u16string& text,
    const AutocompleteMatch& match,
    const GURL& search_url) {
  infobars::ContentInfoBarManager* infobar_manager =
      infobars::ContentInfoBarManager::FromWebContents(web_contents);
  infobar_manager->AddInfoBar(AlternateNavInfoBarDelegate::CreateInfoBar(
      base::WrapUnique(new AlternateNavInfoBarDelegate(
          text, std::make_unique<AutocompleteMatch>(match),
          match.destination_url, search_url))));
}

std::u16string AlternateNavInfoBarDelegate::GetMessageTextWithOffset(
    size_t* link_offset) const {
  const std::u16string label = l10n_util::GetStringFUTF16(
      IDS_ALTERNATE_NAV_URL_VIEW_LABEL, std::u16string(), link_offset);
  return label;
}

infobars::InfoBarDelegate::InfoBarIdentifier
AlternateNavInfoBarDelegate::GetIdentifier() const {
  return ALTERNATE_NAV_INFOBAR_DELEGATE;
}

const gfx::VectorIcon& AlternateNavInfoBarDelegate::GetVectorIcon() const {
  return features::IsRoundedIconsEnabled() ? kGlobeIcon : kGlobeOldIcon;
}

std::u16string AlternateNavInfoBarDelegate::GetLinkText() const {
  return std::u16string();
}

GURL AlternateNavInfoBarDelegate::GetLinkURL() const {
  return destination_url_;
}

bool AlternateNavInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  CHECK(match_, base::NotFatalUntil::M162);
  ChromeOmniboxNavigationObserverBase::OpenAlternativeNavMatch(
      infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar()), text_,
      *match_, original_url_, disposition);

  // We should always close, even if the navigation did not occur within this
  // WebContents.
  return true;
}

std::u16string AlternateNavInfoBarDelegate::GetMessageText() const {
  return l10n_util::GetStringFUTF16(IDS_ALTERNATE_NAV_URL_VIEW_LABEL,
                                    base::UTF8ToUTF16(destination_url_.spec()));
}

std::u16string AlternateNavInfoBarDelegate::GetMessageTextTemplate() const {
  return l10n_util::GetStringUTF16(IDS_ALTERNATE_NAV_URL_VIEW_LABEL);
}

base::span<const MessageSubstitution>
AlternateNavInfoBarDelegate::GetMessageSubstitutions() const {
  return substitutions_;
}

bool AlternateNavInfoBarDelegate::InlineSubstitutionLinkClicked(
    size_t index,
    WindowOpenDisposition disposition) {
  CHECK_EQ(index, 0u);
  return LinkClicked(disposition);
}

int AlternateNavInfoBarDelegate::GetButtons() const {
  return BUTTON_NONE;
}

AlternateNavInfoBarDelegate::AlternateNavInfoBarDelegate(
    const std::u16string& text,
    std::unique_ptr<AutocompleteMatch> match,
    const GURL& destination_url,
    const GURL& original_url)
    : text_(text),
      match_(std::move(match)),
      destination_url_(destination_url),
      original_url_(original_url) {
  if (match_) {
    CHECK_EQ(destination_url_, match_->destination_url,
             base::NotFatalUntil::M162);
  }

  CHECK(destination_url_.is_valid(), base::NotFatalUntil::M162);
  CHECK(original_url_.is_valid(), base::NotFatalUntil::M162);

  substitutions_.emplace_back(base::UTF8ToUTF16(destination_url_.spec()),
                              /*is_link=*/true, std::nullopt);
}

// AlternateNavInfoBarDelegate::CreateInfoBar() is implemented in
// platform-specific files.
