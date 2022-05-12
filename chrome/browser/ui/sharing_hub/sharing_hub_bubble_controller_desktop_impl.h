// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_

#include "base/callback.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"
#include "third_party/blink/public/mojom/opengraph/metadata.mojom-forward.h"
#include "ui/base/models/image_model.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace image_fetcher {
class ImageFetcher;
struct RequestMetadata;
}  // namespace image_fetcher

namespace sharing_hub {

class SharingHubBubbleView;
class SharingHubModel;
struct SharingHubAction;

// Controller component of the Sharing Hub dialog bubble.
// Responsible for showing and hiding an owned bubble.
class SharingHubBubbleControllerDesktopImpl
    : public SharingHubBubbleController,
      public content::WebContentsObserver,
      public content::WebContentsUserData<
          SharingHubBubbleControllerDesktopImpl>,
      public base::SupportsWeakPtr<SharingHubBubbleControllerDesktopImpl> {
 public:
  using PreviewImageChangedCallback =
      base::RepeatingCallback<void(ui::ImageModel)>;

  SharingHubBubbleControllerDesktopImpl(
      const SharingHubBubbleControllerDesktopImpl&) = delete;
  SharingHubBubbleControllerDesktopImpl& operator=(
      const SharingHubBubbleControllerDesktopImpl&) = delete;

  ~SharingHubBubbleControllerDesktopImpl() override;

  // SharingHubBubbleController:
  void HideBubble() override;
  void ShowBubble() override;
  SharingHubBubbleView* sharing_hub_bubble_view() const override;
  bool ShouldOfferOmniboxIcon() override;

  // Returns the title of the Sharing Hub bubble.
  std::u16string GetWindowTitle() const;
  // Returns the current profile.
  Profile* GetProfile() const;

  // Returns the list of Sharing Hub first party actions.
  virtual std::vector<SharingHubAction> GetFirstPartyActions();
  // Returns the list of Sharing Hub third party actions.
  virtual std::vector<SharingHubAction> GetThirdPartyActions();

  virtual bool ShouldUsePreview();
  virtual std::u16string GetPreviewTitle();
  virtual GURL GetPreviewUrl();
  virtual ui::ImageModel GetPreviewImage();

  base::CallbackListSubscription RegisterPreviewImageChangedCallback(
      PreviewImageChangedCallback callback);

  // Handles when the user clicks on a Sharing Hub action. If this is a first
  // party action, executes the appropriate browser command. If this is a third
  // party action, navigates to an external webpage.
  virtual void OnActionSelected(int command_id,
                                bool is_first_party,
                                std::string feature_name_for_metrics);
  // Handler for when the bubble is closed.
  void OnBubbleClosed();

 protected:
  explicit SharingHubBubbleControllerDesktopImpl(
      content::WebContents* web_contents);

 private:
  friend class content::WebContentsUserData<
      SharingHubBubbleControllerDesktopImpl>;

  SharingHubModel* GetSharingHubModel();

  // This method asynchronously fetches the preview image from the page;
  // depending on the UI variant this may be either the favicon or a
  // high-quality preview image supplied by the page. Either way, the resulting
  // image is passed down to the preview view.
  void FetchImageForPreview();

  // This method fetches the webcontents' favicon, if it has one, and updates
  // the preview view to contain it.
  void FetchFaviconForPreview();

  // These three methods handle fetching and displaying high-quality preview
  // images. The first starts the process of fetching the page's OpenGraph
  // metadata. The second receives the resulting metadata and issues a request
  // to fetch and decode the referenced image. The third takes the received HQ
  // preview image and passes it to the preview view for display.
  void FetchHQImageForPreview();
  void OnGetOpenGraphMetadata(blink::mojom::OpenGraphMetadataPtr metadata);
  void OnGetHQImage(const gfx::Image& image,
                    const image_fetcher::RequestMetadata&);

  // Weak reference. Will be nullptr if no bubble is currently shown.
  raw_ptr<SharingHubBubbleView> sharing_hub_bubble_view_ = nullptr;
  // Cached reference to the model.
  raw_ptr<SharingHubModel> sharing_hub_model_ = nullptr;

  base::RepeatingCallbackList<void(ui::ImageModel)>
      preview_image_changed_callbacks_;

  std::unique_ptr<image_fetcher::ImageFetcher> image_fetcher_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
