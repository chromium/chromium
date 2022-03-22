// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_DEFAULT_SEARCH_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_DEFAULT_SEARCH_ICON_SOURCE_H_

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"
#include "url/gurl.h"

class Browser;

namespace gfx {
class Image;
}  // namespace gfx

namespace ui {
class ImageModel;
}  // namespace ui

// A source for the current default search provider's icon image.
class DefaultSearchIconSource : public TemplateURLServiceObserver {
 public:
  using IconChangedSubscription = base::RepeatingClosure;

  DefaultSearchIconSource(Browser* browser,
                          IconChangedSubscription icon_changed_subscription);
  DefaultSearchIconSource(const DefaultSearchIconSource&) = delete;
  DefaultSearchIconSource& operator=(const DefaultSearchIconSource&) = delete;
  ~DefaultSearchIconSource() override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Gets the icon image for the current default search provider with padding
  // added to bring the resulting ImageModel up to `size`.
  ui::ImageModel GetSizedIconImage(int size) const;

  // Similar to `GetSizedIconImage()` except this does not add padding.
  ui::ImageModel GetIconImage() const;

 private:
  // Callback used for when `GetSizedIconImage()` does not return the icon image
  // immediately but instead fetches the image asynchronously.
  void OnIconFetched(const gfx::Image& icon);

  // Gets the raw gfx::Image icon from the TemplateURL for the current default
  // search provider. Will return an empty image if this misses in the icon
  // cache and instead will notify the `icon_changed_subscription_` when the
  // icon is ready. FaviconCache guarantees favicons will be of size
  // gfx::kFaviconSize (16x16)
  gfx::Image GetRawIconImage() const;

  // Used to fetch the ChromeOmniboxClient associated with the browser's active
  // tab.
  raw_ptr<Browser> browser_;

  // Called whenever the state of the TemplateURLService changes, resulting in a
  // call back into `OnFaviconFetched()`.
  IconChangedSubscription icon_changed_subscription_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  base::WeakPtrFactory<DefaultSearchIconSource> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_DEFAULT_SEARCH_ICON_SOURCE_H_
