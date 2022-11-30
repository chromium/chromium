// Copyright 2022 The Chromium Authors
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
          SharingHubBubbleControllerDesktopImpl> {
 public:
  SharingHubBubbleControllerDesktopImpl(
      const SharingHubBubbleControllerDesktopImpl&) = delete;
  SharingHubBubbleControllerDesktopImpl& operator=(
      const SharingHubBubbleControllerDesktopImpl&) = delete;

  ~SharingHubBubbleControllerDesktopImpl() override;

  // SharingHubBubbleController:
  void HideBubble() override;
  void ShowBubble(share::ShareAttempt attempt) override;
  SharingHubBubbleView* sharing_hub_bubble_view() const override;
  bool ShouldOfferOmniboxIcon() override;

  // Returns the title of the Sharing Hub bubble.
  std::u16string GetWindowTitle() const;
  // Returns the current profile.
  Profile* GetProfile() const;

  // SharingHubBubbleController:
  std::vector<SharingHubAction> GetFirstPartyActions() override;
  std::vector<SharingHubAction> GetThirdPartyActions() override;
  bool ShouldUsePreview() override;
  base::CallbackListSubscription RegisterPreviewImageChangedCallback(
      PreviewImageChangedCallback callback) override;
  base::WeakPtr<SharingHubBubbleController> GetWeakPtr() override;
  void OnActionSelected(int command_id,
                        bool is_first_party,
                        std::string feature_name_for_metrics) override;
  void OnBubbleClosed() override;

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

  base::WeakPtrFactory<SharingHubBubbleController> weak_factory_{this};

  // This is a bit ugly: SharingHubBubbleController's interface requires it to
  // be able to create WeakPtr<SharingHubBubbleController>, but this class
  // internally also needs to be able to bind weak pointers to itself for use
  // with the image fetching state machine. Those internal weak pointers need to
  // be to an instance of *this class*, not of the parent interface, so that we
  // can bind them to methods on this class rather than the parent interface.
  base::WeakPtrFactory<SharingHubBubbleControllerDesktopImpl>
      internal_weak_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_BUBBLE_CONTROLLER_DESKTOP_IMPL_H_
