// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_search/default_search_icon_source.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "components/omnibox/browser/omnibox_controller.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/canvas_image_source.h"

DefaultSearchIconSource::~DefaultSearchIconSource() = default;

void DefaultSearchIconSource::OnTemplateURLServiceChanged() {
  callback_list_.Notify();
}

void DefaultSearchIconSource::OnTemplateURLServiceShuttingDown() {
  template_url_service_observation_.Reset();
}

ui::ImageModel DefaultSearchIconSource::GetSizedIconImage(int size) const {
  // If `icon` is empty we may have missed in the cache. Early return and notify
  // clients when the icon is ready.
  gfx::Image icon = GetRawIconImage();
  if (icon.IsEmpty())
    return ui::ImageModel();

  // FaviconCache guarantee favicons will be of size gfx::kFaviconSize (16x16)
  // so add extra padding around them to align them vertically with the other
  // vector icons.
  DCHECK_GE(size, icon.Height());
  DCHECK_GE(size, icon.Width());
  auto padding_border =
      gfx::Insets::VH((size - icon.Height()) / 2, (size - icon.Width()) / 2);

  return padding_border.IsEmpty()
             ? ui::ImageModel::FromImage(icon)
             : ui::ImageModel::FromImageSkia(
                   gfx::CanvasImageSource::CreatePadded(*icon.ToImageSkia(),
                                                        padding_border));
}

ui::ImageModel DefaultSearchIconSource::GetIconImage() const {
  return ui::ImageModel::FromImage(GetRawIconImage());
}

base::CallbackListSubscription
DefaultSearchIconSource::RegisterIconChangedSubscription(
    IconChangedSubscription icon_changed_subscription) {
  return callback_list_.Add(std::move(icon_changed_subscription));
}

DefaultSearchIconSource::DefaultSearchIconSource(Browser* browser)
    : BrowserUserData<DefaultSearchIconSource>(*browser) {
  // `template_url_service` may be null in tests.
  if (auto* template_url_service =
          TemplateURLServiceFactory::GetForProfile(browser->profile())) {
    template_url_service_observation_.Observe(template_url_service);
  }
}

gfx::Image DefaultSearchIconSource::GetRawIconImage() const {
  content::WebContents* active_contents =
      GetBrowser().tab_strip_model()->GetActiveWebContents();
  if (!active_contents)
    return gfx::Image();

  // Attempt to synchronously get the current default search engine's favicon.
  auto* omnibox_view = search::GetOmniboxView(active_contents);
  DCHECK(omnibox_view);
  return omnibox_view->controller()
      ->client()
      ->GetFaviconForDefaultSearchProvider(
          base::BindRepeating(&DefaultSearchIconSource::OnIconFetched,
                              weak_ptr_factory_.GetMutableWeakPtr()));
}

void DefaultSearchIconSource::OnIconFetched(const gfx::Image& icon) {
  // The favicon requested in the call to GetFaviconForDefaultSearchProvider()
  // will now have been cached by ChromeOmniboxClient's FaviconCache and
  // subsequent calls asking for the favicon will now return synchronously.
  // Notify clients so they can attempt to fetch the latest icon.
  callback_list_.Notify();
}

BROWSER_USER_DATA_KEY_IMPL(DefaultSearchIconSource);
