// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/image_service/image_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/commerce/price_tracking/shopping_list_ui_tab_helper.h"
#include "chrome/browser/ui/sync/sync_promo_ui.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/commerce/price_tracking_view.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/page_image_service/image_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

#if !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/sync/bubble_sync_promo_view.h"
#endif

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// TODO(pbos): Investigate replacing this with a views-agnostic
// BookmarkBubbleDelegate.
views::BubbleDialogDelegate* BookmarkBubbleView::bookmark_bubble_ = nullptr;

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBookmarkName);
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kBookmarkFolder);

void FetchImageForUrl(const GURL& url, Profile* profile) {
  page_image_service::ImageService* image_service =
      page_image_service::ImageServiceFactory::GetForBrowserContext(profile);
  if (!image_service) {
    return;
  }
  page_image_service::mojom::Options options;
  options.suggest_images = true;
  options.optimization_guide_images = true;
  image_service->FetchImageFor(
      page_image_service::mojom::ClientId::Bookmarks, url, options,
      base::BindOnce(&BookmarkBubbleView::HandleImageUrlResponse, profile));
}

gfx::ImageSkia GetFaviconForWebContents(content::WebContents* web_contents) {
  const auto& color_provider = web_contents->GetColorProvider();
  const gfx::Image url_favicon =
      favicon::TabFaviconFromWebContents(web_contents);
  gfx::ImageSkia favicon =
      url_favicon.IsEmpty()
          ? favicon::GetDefaultFaviconModel(ui::kColorBubbleBackground)
                .Rasterize(&color_provider)
          : url_favicon.AsImageSkia();
  const bool is_dark =
      color_utils::IsDark(color_provider.GetColor(ui::kColorBubbleBackground));
  const SkColor background_color = is_dark ? SK_ColorBLACK : SK_ColorWHITE;
  const bool themify_favicon =
      web_contents->GetURL().SchemeIs(content::kChromeUIScheme);
  if (themify_favicon) {
    favicon = favicon::ThemeFavicon(favicon, SK_ColorWHITE, background_color,
                                    background_color);
  }
  constexpr int kMainImageDimension = 112;
  gfx::ImageSkia centered_favicon =
      gfx::ImageSkiaOperations::CreateImageWithRoundRectBackground(
          kMainImageDimension, 0, background_color, favicon);
  return centered_favicon;
}
}  // namespace

class BookmarkBubbleView::BookmarkBubbleDelegate
    : public ui::DialogModelDelegate {
 public:
  BookmarkBubbleDelegate(std::unique_ptr<BubbleSyncPromoDelegate> delegate,
                         Browser* browser,
                         const GURL& url)
      : delegate_(std::move(delegate)), browser_(browser), url_(url) {}

  void RemoveBookmark() {
    base::RecordAction(UserMetricsAction("BookmarkBubble_Unstar"));
    should_apply_edits_ = false;
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser_->profile());
    const bookmarks::BookmarkNode* node =
        model->GetMostRecentlyAddedUserNodeForURL(url_);
    if (node)
      model->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser);
  }

  void OnWindowClosing() {
    if (should_apply_edits_)
      ApplyEdits();
    bookmark_bubble_ = nullptr;
  }

  void OnEditButton(const ui::Event& event) {
    base::RecordAction(UserMetricsAction("BookmarkBubble_Edit"));
    ShowEditor();
  }

  void ShowEditor() {
    DCHECK(dialog_model()->host());

    Profile* const profile = browser_->profile();

    const bookmarks::BookmarkNode* node =
        BookmarkModelFactory::GetForBrowserContext(profile)
            ->GetMostRecentlyAddedUserNodeForURL(url_);
    DCHECK(bookmark_bubble_->anchor_widget());
    gfx::NativeWindow native_parent =
        bookmark_bubble_->anchor_widget()->GetNativeWindow();
    DCHECK(native_parent);

    // Note that closing the dialog with |should_apply_edits_| still true will
    // synchronously save any pending changes.
    dialog_model()->host()->Close();

    if (node && native_parent) {
      BookmarkEditor::Show(native_parent, profile,
                           BookmarkEditor::EditDetails::EditNode(node),
                           BookmarkEditor::SHOW_TREE);
    }
  }

  void OnComboboxAction() {
    const auto* combobox =
        dialog_model()->GetComboboxByUniqueId(kBookmarkFolder);
    if (static_cast<size_t>(combobox->selected_index()) + 1 ==
        GetFolderModel()->GetItemCount()) {
      base::RecordAction(UserMetricsAction("BookmarkBubble_EditFromCombobox"));
      ShowEditor();
    }
  }

  void ApplyEdits() {
    DCHECK(should_apply_edits_);
    // Set this to make sure we don't attempt to apply edits again.
    should_apply_edits_ = false;

    bookmarks::BookmarkModel* const model =
        BookmarkModelFactory::GetForBrowserContext(browser_->profile());
    const bookmarks::BookmarkNode* node =
        model->GetMostRecentlyAddedUserNodeForURL(url_);
    if (!node)
      return;
    const std::u16string new_title =
        dialog_model()->GetTextfieldByUniqueId(kBookmarkName)->text();
    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title,
                      bookmarks::metrics::BookmarkEditSource::kUser);
      base::RecordAction(
          UserMetricsAction("BookmarkBubble_ChangeTitleInBubble"));
    }

    GetFolderModel()->MaybeChangeParent(
        node, dialog_model()
                  ->GetComboboxByUniqueId(kBookmarkFolder)
                  ->selected_index());

    if (base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel)) {
      browser_->window()->MaybeShowFeaturePromo(
          feature_engagement::kIPHPowerBookmarksSidePanelFeature);
    }
  }

  RecentlyUsedFoldersComboModel* GetFolderModel() {
    DCHECK(dialog_model());
    return static_cast<RecentlyUsedFoldersComboModel*>(
        dialog_model()
            ->GetComboboxByUniqueId(kBookmarkFolder)
            ->combobox_model());
  }

  BubbleSyncPromoDelegate* delegate() { return delegate_.get(); }

 private:
  std::unique_ptr<BubbleSyncPromoDelegate> delegate_;
  const raw_ptr<Browser> browser_;
  const GURL url_;

  bool should_apply_edits_ = true;
};

// static
void BookmarkBubbleView::ShowBubble(
    views::View* anchor_view,
    content::WebContents* web_contents,
    views::Button* highlighted_button,
    std::unique_ptr<BubbleSyncPromoDelegate> delegate,
    Browser* browser,
    const GURL& url,
    bool already_bookmarked) {
  if (bookmark_bubble_)
    return;
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  BubbleSyncPromoDelegate* const delegate_ptr = delegate.get();
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
  Profile* profile = browser->profile();
  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(url);

  auto bubble_delegate_unique = std::make_unique<BookmarkBubbleDelegate>(
      std::move(delegate), browser, url);
  BookmarkBubbleDelegate* bubble_delegate = bubble_delegate_unique.get();

  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);
  absl::optional<commerce::ProductInfo> product_info = absl::nullopt;
  gfx::Image product_image;
  if (shopping_service->IsShoppingListEligible()) {
    product_info = shopping_service->GetAvailableProductInfoForUrl(url);
    auto* tab_helper =
        commerce::ShoppingListUiTabHelper::FromWebContents(web_contents);
    CHECK(tab_helper);

    product_image = tab_helper->GetProductImage();
  }

  auto dialog_model_builder =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique));
  if (base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel)) {
    gfx::ImageSkia main_image = product_image.AsImageSkia();

    if (product_image.IsEmpty()) {
      // Fetch image from ImageService asynchronously
      FetchImageForUrl(url, profile);
      // Display favicon while awaiting ImageService response
      const auto centered_favicon = GetFaviconForWebContents(web_contents);
      main_image = centered_favicon;
    }

    dialog_model_builder.SetMainImage(
        ui::ImageModel::FromImageSkia(main_image));
  } else {
    dialog_model_builder.AddExtraButton(
        base::BindRepeating(&BookmarkBubbleDelegate::OnEditButton,
                            base::Unretained(bubble_delegate)),
        ui::DialogModelButton::Params()
            .SetLabel(l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_OPTIONS))
            .AddAccelerator(ui::Accelerator(ui::VKEY_E, ui::EF_ALT_DOWN)));
  }
  dialog_model_builder
      .SetTitle(l10n_util::GetStringUTF16(
          already_bookmarked ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK
                             : IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED))
      .SetDialogDestroyingCallback(
          base::BindOnce(&BookmarkBubbleDelegate::OnWindowClosing,
                         base::Unretained(bubble_delegate)))
      .AddOkButton(base::BindOnce(&BookmarkBubbleDelegate::ApplyEdits,
                                  base::Unretained(bubble_delegate)),
                   ui::DialogModelButton::Params().SetLabel(
                       l10n_util::GetStringUTF16(IDS_DONE)))
      .AddCancelButton(
          base::BindOnce(&BookmarkBubbleDelegate::RemoveBookmark,
                         base::Unretained(bubble_delegate)),
          ui::DialogModelButton::Params()
              .SetLabel(l10n_util::GetStringUTF16(
                  IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK))
              .AddAccelerator(ui::Accelerator(ui::VKEY_R, ui::EF_ALT_DOWN)))
      .AddTextfield(
          kBookmarkName,
          l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_NAME_LABEL),
          bookmark_node->GetTitle(),
          ui::DialogModelTextfield::Params().SetAccessibleName(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_BUBBLE_NAME_LABEL)))
      .AddCombobox(
          kBookmarkFolder,
          l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_FOLDER_LABEL),
          std::make_unique<RecentlyUsedFoldersComboModel>(
              bookmark_model,
              bookmark_model->GetMostRecentlyAddedUserNodeForURL(url)),
          ui::DialogModelCombobox::Params().SetCallback(
              base::BindRepeating(&BookmarkBubbleDelegate::OnComboboxAction,
                                  base::Unretained(bubble_delegate))))
      .SetInitiallyFocusedField(kBookmarkName);

  if (commerce::CanTrackPrice(product_info) && !product_image.IsEmpty()) {
    bool is_price_tracked = shopping_service->IsSubscribedFromCache(
        commerce::BuildUserSubscriptionForClusterId(
            product_info->product_cluster_id.value()));
    if (!base::FeatureList::IsEnabled(features::kPowerBookmarksSidePanel)) {
      dialog_model_builder.AddSeparator();
    }
    dialog_model_builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            std::make_unique<PriceTrackingView>(
                profile, url, *product_image.ToImageSkia(), is_price_tracked),
            views::BubbleDialogModelHost::FieldType::kControl),
        kPriceTrackingBookmarkViewElementId);
  }

  // views:: land below, there's no agnostic reference to arrow / anchors /
  // bubbles.
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_model_builder.Build(), anchor_view,
      views::BubbleBorder::TOP_RIGHT);
  bookmark_bubble_ = bubble.get();
  if (highlighted_button)
    bubble->SetHighlightedButton(highlighted_button);

#if !BUILDFLAG(IS_CHROMEOS_ASH)
  if (SyncPromoUI::ShouldShowSyncPromo(profile)) {
    // TODO(pbos): Consider adding model support for footnotes so that this does
    // not need to be tied to views.
    // TODO(pbos): Consider updating ::SetFootnoteView so that it can resize the
    // widget to account for it.
    bubble->SetFootnoteView(std::make_unique<BubbleSyncPromoView>(
        profile, delegate_ptr,
        signin_metrics::AccessPoint::ACCESS_POINT_BOOKMARK_BUBBLE,
        IDS_BOOKMARK_DICE_PROMO_SYNC_MESSAGE,
        /*dice_signin_button_prominent=*/false));
  }
#endif

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  widget->Show();
}

// static
void BookmarkBubbleView::Hide() {
  if (bookmark_bubble_)
    bookmark_bubble_->GetWidget()->Close();
}

// static
void BookmarkBubbleView::HandleImageUrlResponse(const Profile* profile,
                                                const GURL& image_service_url) {
  if (!image_service_url.is_empty()) {
    constexpr net::NetworkTrafficAnnotationTag kTrafficAnnotation =
        net::DefineNetworkTrafficAnnotation("bookmarks_image_fetcher",
                                            R"(
        semantics {
          sender: "Image fetcher for the bookmarks feature."
          description:
            "Retrieves an image that is representative of the active webpage, "
            "base on heuristics. The image is fetched from Google servers. "
            "This will be shown to the user as part of the bookmarking action."
          trigger:
            "When adding a new bookmark, we will attempt to fetch the "
            "image for it."
          data:
            "A gstatic URL for an image on the active web page"
          destination: GOOGLE_OWNED_SERVICE
          internal {
            contacts {
              email: "chrome-desktop-ui-sea@google.com"
            }
          }
          user_data {
            type: SENSITIVE_URL
          }
          last_reviewed: "2023-03-24"
        }
        policy {
          cookies_allowed: NO
          setting:
            "This fetch is enabled for any user with the Bookmarks sync"
            "feature enabled."
          chrome_policy {
            SyncDisabled {
              SyncDisabled: true
            }
            SyncTypesListDisabled {
              SyncTypesListDisabled: {
                entries: "bookmarks"
              }
            }
          }
        })");

    constexpr char kImageFetcherUmaClient[] = "Bookmarks";

    image_fetcher::ImageFetcher* fetcher =
        ImageFetcherServiceFactory::GetForKey(profile->GetProfileKey())
            ->GetImageFetcher(image_fetcher::ImageFetcherConfig::kNetworkOnly);
    fetcher->FetchImage(image_service_url,
                        base::BindOnce(&HandleImageBytesResponse),
                        image_fetcher::ImageFetcherParams(
                            kTrafficAnnotation, kImageFetcherUmaClient));
  }
}

// static
void BookmarkBubbleView::HandleImageBytesResponse(
    const gfx::Image& image,
    const image_fetcher::RequestMetadata& metadata) {
  if (!image.IsEmpty() && BookmarkBubbleView::bookmark_bubble()) {
    BookmarkBubbleView::bookmark_bubble()->SetMainImage(
        ui::ImageModel::FromImage(image));
  }
}
