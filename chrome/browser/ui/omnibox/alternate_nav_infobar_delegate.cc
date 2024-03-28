// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/omnibox/alternate_nav_infobar_delegate.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/autocomplete/shortcuts_backend_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/history/core/browser/history_service.h"
#include "components/infobars/content/content_infobar_manager.h"
#include "components/infobars/core/infobar.h"
#include "components/omnibox/browser/shortcuts_backend.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"

AlternateNavInfoBarDelegate::~AlternateNavInfoBarDelegate() {
}

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
          Profile::FromBrowserContext(web_contents->GetBrowserContext()), text,
          std::make_unique<AutocompleteMatch>(match), match.destination_url,
          search_url))));
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
  return kGlobeIcon;
}

std::u16string AlternateNavInfoBarDelegate::GetLinkText() const {
  return base::UTF8ToUTF16(destination_url_.spec());
}

GURL AlternateNavInfoBarDelegate::GetLinkURL() const {
  return destination_url_;
}

bool AlternateNavInfoBarDelegate::LinkClicked(
    WindowOpenDisposition disposition) {
  DCHECK(match_);
  history::HistoryService* const history_service =
      HistoryServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::IMPLICIT_ACCESS);

  // Tell the shortcuts backend to remove the shortcut it added for the
  // original search and instead add one reflecting this navigation.
  scoped_refptr<ShortcutsBackend> shortcuts_backend(
      ShortcutsBackendFactory::GetForProfile(profile_));
  if (shortcuts_backend.get()) {  // May be NULL in incognito.
    shortcuts_backend->DeleteShortcutsWithURL(original_url_);
    shortcuts_backend->AddOrUpdateShortcut(text_, *match_);
  }

  // Tell the history system to remove any saved search term for the search.
  if (history_service)
    history_service->DeleteKeywordSearchTermForURL(original_url_);

  // Pretend the user typed this URL, so that navigating to it will be the
  // default action when it's typed again in the future.
  infobars::ContentInfoBarManager::WebContentsFromInfoBar(infobar())->OpenURL(
      content::OpenURLParams(destination_url_, content::Referrer(), disposition,
                             ui::PAGE_TRANSITION_TYPED, false),
      /*navigation_handle_callback=*/{});

  // We should always close, even if the navigation did not occur within this
  // WebContents.
  return true;
}

AlternateNavInfoBarDelegate::AlternateNavInfoBarDelegate(
    Profile* profile,
    const std::u16string& text,
    std::unique_ptr<AutocompleteMatch> match,
    const GURL& destination_url,
    const GURL& original_url)
    : infobars::InfoBarDelegate(),
      profile_(profile),
      text_(text),
      match_(std::move(match)),
      destination_url_(destination_url),
      original_url_(original_url) {
  if (match_)
    DCHECK_EQ(destination_url_, match_->destination_url);

  DCHECK(destination_url_.is_valid());
  DCHECK(original_url_.is_valid());
}

// AlternateNavInfoBarDelegate::CreateInfoBar() is implemented in
// platform-specific files.
