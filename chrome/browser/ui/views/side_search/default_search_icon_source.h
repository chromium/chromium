// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_DEFAULT_SEARCH_ICON_SOURCE_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_DEFAULT_SEARCH_ICON_SOURCE_H_

#include "base/callback_list.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_user_data.h"
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
// TODO(tluk): This should be updated to be a per-profile rather than
// per-browser. Currently this is per-browser only due to the fact we are
// leveraging the OmniboxClient interface, which is instantiated on a
// per-browser-window basis.
class DefaultSearchIconSource : public BrowserUserData<DefaultSearchIconSource>,
                                public TemplateURLServiceObserver {
 public:
  using IconChangedSubscription = base::RepeatingClosure;
  using CallbackList = base::RepeatingCallbackList<void()>;

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

  // Registers the provided IconChangedSubscription. Destroying the returned
  // subscription will unregister the callback. This is safe to do while in the
  // context of the callback itself.
  base::CallbackListSubscription RegisterIconChangedSubscription(
      IconChangedSubscription icon_changed_subscription);

 private:
  friend BrowserUserData;

  explicit DefaultSearchIconSource(Browser* browser);

  // Callback used for when `GetSizedIconImage()` does not return the icon image
  // immediately but instead fetches the image asynchronously.
  void OnIconFetched(const gfx::Image& icon);

  // Gets the raw gfx::Image icon from the TemplateURL for the current default
  // search provider. Will return an empty image if this misses in the icon
  // cache and instead will notify the `icon_changed_subscription_` when the
  // icon is ready. FaviconCache guarantees favicons will be of size
  // gfx::kFaviconSize (16x16)
  gfx::Image GetRawIconImage() const;

  // Callbacks are notified whenever the state of the TemplateURLService
  // changes, resulting in a call back into `OnFaviconFetched()`.
  CallbackList callback_list_;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};

  base::WeakPtrFactory<DefaultSearchIconSource> weak_ptr_factory_{this};

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_SEARCH_DEFAULT_SEARCH_ICON_SOURCE_H_
