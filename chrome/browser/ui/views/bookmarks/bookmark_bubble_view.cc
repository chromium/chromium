// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/bookmarks/bookmark_bubble_view.h"

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/commerce/shopping_service_factory.h"
#include "chrome/browser/favicon/favicon_utils.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/image_fetcher/image_fetcher_service_factory.h"
#include "chrome/browser/page_image_service/image_service_factory.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/signin/chrome_signin_client.h"
#include "chrome/browser/signin/signin_promo_util.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/bookmarks/bookmark_editor.h"
#include "chrome/browser/ui/bookmarks/recently_used_folders_combo_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/page_action/page_action_icon_type.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/commerce/price_tracking_email_dialog_view.h"
#include "chrome/browser/ui/views/commerce/price_tracking_view.h"
#include "chrome/browser/ui/views/commerce/shopping_collection_iph_view.h"
#include "chrome/browser/ui/views/location_bar/star_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/metrics/metrics_utils.h"
#include "components/commerce/core/price_tracking_utils.h"
#include "components/commerce/core/shopping_service.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/image_fetcher/core/image_fetcher.h"
#include "components/image_fetcher/core/image_fetcher_service.h"
#include "components/page_image_service/image_service.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/service/local_data_description.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/image_model.h"
#include "ui/base/ui_base_features.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/controls/styled_label.h"

#if !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "chrome/browser/ui/views/bookmarks/bookmark_sign_in_promo_bubble_view.h"
#include "components/sync/base/features.h"
#endif

using base::UserMetricsAction;
using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;

// TODO(pbos): Investigate replacing this with a views-agnostic
// BookmarkBubbleDelegate.
views::BubbleDialogDelegate* BookmarkBubbleView::bookmark_bubble_ = nullptr;

DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkBubbleOkButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkFolderFieldId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkNameFieldId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkSecondaryButtonId);
DEFINE_ELEMENT_IDENTIFIER_VALUE(kBookmarkBubbleFrameViewId);

namespace {

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
          gfx::SizeF(kMainImageDimension, kMainImageDimension), /*radius=*/0,
          background_color, favicon);
  return centered_favicon;
}

base::OnceCallback<void()> CreatePriceTrackingEmailCallback(
    Profile* profile,
    views::View* anchor_view,
    content::WebContents* web_contents,
    const bookmarks::BookmarkNode* bookmark) {
  if (!profile ||
      commerce::IsEmailNotificationPrefSetByUser(profile->GetPrefs())) {
    return base::DoNothing();
  }

  // Make sure we don't over-trigger the dialog.
  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);
  if (!tracker ||
      !tracker->ShouldTriggerHelpUI(
          feature_engagement::kIPHPriceTrackingEmailConsentFeature)) {
    return base::DoNothing();
  }

  base::OnceCallback<void()> show_dialog_callback = base::BindOnce(
      [](base::WeakPtr<content::WebContents> web_contents, Profile* profile,
         views::View* anchor) {
        if (!web_contents || !profile || !anchor) {
          return;
        }
        PriceTrackingEmailDialogCoordinator(anchor).Show(
            web_contents.get(), profile, base::DoNothing());
      },
      web_contents->GetWeakPtr(), profile, anchor_view);

  return base::BindOnce(
      [](Profile* profile, const bookmarks::BookmarkNode* node,
         base::OnceCallback<void()> show_dialog) {
        commerce::IsBookmarkPriceTracked(
            commerce::ShoppingServiceFactory::GetForBrowserContext(profile),
            BookmarkModelFactory::GetForBrowserContext(profile), node,
            base::BindOnce(
                [](base::OnceCallback<void()> show_dialog, bool is_tracked) {
                  if (is_tracked) {
                    std::move(show_dialog).Run();
                  }
                },
                std::move(show_dialog)));
      },
      profile, bookmark, std::move(show_dialog_callback));
}

bool ShouldShowShoppingCollectionFootnote(Profile* profile,
                                          bookmarks::BookmarkModel* model,
                                          const bookmarks::BookmarkNode* node) {
  if (!commerce::IsProductBookmark(model, node)) {
    return false;
  }

  // Only show the IPH if the bookmark was saved to the shopping collection.
  const bookmarks::BookmarkNode* collection =
      commerce::GetShoppingCollectionBookmarkFolder(model);
  if (!collection || node->parent()->uuid() != collection->uuid()) {
    return false;
  }

  auto* tracker =
      feature_engagement::TrackerFactory::GetForBrowserContext(profile);

  if (!tracker || !tracker->ShouldTriggerHelpUI(
                      feature_engagement::kIPHShoppingCollectionFeature)) {
    return false;
  }

  // Immediately dismiss the explainer so that it doesn't prevent the IPH
  // for other features from showing.
  tracker->Dismissed(feature_engagement::kIPHShoppingCollectionFeature);

  return true;
}

actions::ActionItem& GetBookmarkActionItem(BrowserWindowInterface* bwi) {
  CHECK(bwi);
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionBookmarkThisTab, bwi->GetActions()->root_action_item());
  CHECK(action_item);
  return *action_item;
}

#if !BUILDFLAG(IS_CHROMEOS)
void MaybeShowSignInPromo(bool already_bookmarked,
                          Profile* profile,
                          views::View* anchor_view,
                          content::WebContents* web_contents,
                          const bookmarks::BookmarkNode* bookmark) {
  if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
    return;
  }

  if (!anchor_view) {
    return;
  }

  if (!signin::ShouldShowBookmarkSignInPromo(*profile)) {
    return;
  }

  if (!bookmark || already_bookmarked) {
    return;
  }

  BookmarkSigninPromoBubbleView* bubble =
      new BookmarkSigninPromoBubbleView(anchor_view, web_contents, bookmark);
  views::BubbleDialogDelegateView::CreateBubble(bubble);
  bubble->ShowForReason(LocationBarBubbleDelegateView::USER_GESTURE);
}
#endif

}  // namespace

class BookmarkBubbleView::BookmarkBubbleDelegate
    : public ui::DialogModelDelegate {
 public:
  BookmarkBubbleDelegate(Browser* browser, const GURL& url)
      : browser_(browser),
        url_(url),
        action_item_(GetBookmarkActionItem(browser)) {
    action_item_->SetIsShowingBubble(true);
  }

  // Handles presses on the secondary (usually cancel) button and returns
  // whether the dialog should close as a result of the button press. In this
  // case, the button is either a "cancel" button where pressing will close
  // the dialog or an "edit" button which shows more options and then transforms
  // into the cancel button.
  bool HandleSecondaryButton() {
    base::RecordAction(UserMetricsAction("BookmarkBubble_Unstar"));
    should_apply_edits_ = false;
    bookmarks::BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(browser_->profile());
    const bookmarks::BookmarkNode* node =
        model->GetMostRecentlyAddedUserNodeForURL(url_);
    if (node) {
      model->Remove(node, bookmarks::metrics::BookmarkEditSource::kUser,
                    FROM_HERE);
    }

    return true;
  }

  void SetCloseCallback(base::OnceCallback<void()> close_callback) {
    close_callback_ = std::move(close_callback);
  }

  void OnWindowClosing() {
    if (should_apply_edits_) {
      ApplyEdits();
    }
    action_item_->SetIsShowingBubble(false);
    bookmark_bubble_ = nullptr;

    if (close_callback_) {
      std::move(close_callback_).Run();
    }
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
        dialog_model()->GetComboboxByUniqueId(kBookmarkFolderFieldId);
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
    if (!node) {
      return;
    }
    const std::u16string new_title =
        dialog_model()->GetTextfieldByUniqueId(kBookmarkNameFieldId)->text();
    if (new_title != node->GetTitle()) {
      model->SetTitle(node, new_title,
                      bookmarks::metrics::BookmarkEditSource::kUser);
      base::RecordAction(
          UserMetricsAction("BookmarkBubble_ChangeTitleInBubble"));
    }

    GetFolderModel()->MaybeChangeParent(
        node, dialog_model()
                  ->GetComboboxByUniqueId(kBookmarkFolderFieldId)
                  ->selected_index());

    BrowserUserEducationInterface::From(browser_)->MaybeShowFeaturePromo(
        feature_engagement::kIPHPowerBookmarksSidePanelFeature);
  }

  RecentlyUsedFoldersComboModel* GetFolderModel() {
    DCHECK(dialog_model());
    return static_cast<RecentlyUsedFoldersComboModel*>(
        dialog_model()
            ->GetComboboxByUniqueId(kBookmarkFolderFieldId)
            ->combobox_model());
  }

 private:
  const raw_ptr<Browser> browser_;
  const GURL url_;
  base::OnceCallback<void()> close_callback_;

  bool should_apply_edits_ = true;
  const raw_ref<actions::ActionItem> action_item_;
};

// static
void BookmarkBubbleView::ShowBubble(views::View* anchor_view,
                                    content::WebContents* web_contents,
                                    views::Button* highlighted_button,
                                    Browser* browser,
                                    const GURL& url,
                                    bool already_bookmarked) {
  // The only point where the star view can properly observe the bubble dialog
  // delegate's widget is in this function, that's why star view is observing
  // the widget from here after its creation.
  // This is only neceessary for the legacy page action framework.
  StarView* star_view = nullptr;
  if (!IsPageActionMigrated(PageActionIconType::kBookmarkStar)) {
    star_view = static_cast<StarView*>(highlighted_button);
  }
  if (bookmark_bubble_) {
    if (star_view) {
      star_view->OnBubbleWidgetChanged(bookmark_bubble_->GetWidget());
    }
    return;
  }
  Profile* profile = browser->profile();
  CHECK(profile);
  CHECK(web_contents);

  bookmarks::BookmarkModel* bookmark_model =
      BookmarkModelFactory::GetForBrowserContext(profile);
  const bookmarks::BookmarkNode* bookmark_node =
      bookmark_model->GetMostRecentlyAddedUserNodeForURL(url);

  commerce::ShoppingService* shopping_service =
      commerce::ShoppingServiceFactory::GetForBrowserContext(profile);

  base::OnceCallback<void()> post_save_callback =
      CreatePriceTrackingEmailCallback(profile, anchor_view, web_contents,
                                       bookmark_node);

  auto bubble_delegate_unique =
      std::make_unique<BookmarkBubbleDelegate>(browser, url);
  BookmarkBubbleDelegate* bubble_delegate = bubble_delegate_unique.get();

  auto dialog_model_builder =
      ui::DialogModel::Builder(std::move(bubble_delegate_unique));

  std::optional<commerce::ProductInfo> product_info = std::nullopt;
  if (shopping_service->IsShoppingListEligible()) {
    product_info = shopping_service->GetAvailableProductInfoForUrl(url);
  }

  if (product_info.has_value() && !product_info->image_url.is_empty()) {
    HandleImageUrlResponse(profile, product_info->image_url);
  } else {
    // Fetch image from ImageService asynchronously
    FetchImageForUrl(url, profile);
  }

  // Display favicon while awaiting one of the above options to load.
  const auto centered_favicon = GetFaviconForWebContents(web_contents);
  gfx::ImageSkia main_image = centered_favicon;

  dialog_model_builder.SetMainImage(ui::ImageModel::FromImageSkia(main_image));

  ui::ElementIdentifier initially_focused_field = kBookmarkNameFieldId;
  std::u16string secondary_button_label =
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_REMOVE_BOOKMARK);

  dialog_model_builder
      .SetTitle(l10n_util::GetStringUTF16(
          already_bookmarked ? IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARK
                             : IDS_BOOKMARK_BUBBLE_PAGE_BOOKMARKED))
      .SetDialogDestroyingCallback(
          base::BindOnce(&BookmarkBubbleDelegate::OnWindowClosing,
                         base::Unretained(bubble_delegate)))
      .AddOkButton(base::BindOnce(&BookmarkBubbleDelegate::ApplyEdits,
                                  base::Unretained(bubble_delegate))
#if !BUILDFLAG(IS_CHROMEOS)
                       .Then(base::BindOnce(
                           MaybeShowSignInPromo, already_bookmarked, profile,
                           anchor_view, web_contents, bookmark_node))
#endif
                       ,
                   ui::DialogModel::Button::Params()
                       .SetLabel(l10n_util::GetStringUTF16(IDS_DONE))
                       .SetId(kBookmarkBubbleOkButtonId))
      .AddCancelButton(
          base::BindRepeating(&BookmarkBubbleDelegate::HandleSecondaryButton,
                              base::Unretained(bubble_delegate)),
          ui::DialogModel::Button::Params()
              .SetLabel(secondary_button_label)
              .SetStyle(ui::ButtonStyle::kTonal)
              .AddAccelerator(ui::Accelerator(ui::VKEY_R, ui::EF_ALT_DOWN))
              .SetId(kBookmarkSecondaryButtonId))
      .AddTextfield(
          kBookmarkNameFieldId,
          l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_NAME_LABEL),
          bookmark_node->GetTitle(),
          ui::DialogModelTextfield::Params().SetAccessibleName(
              l10n_util::GetStringUTF16(IDS_BOOKMARK_AX_BUBBLE_NAME_LABEL)))
      .AddCombobox(
          kBookmarkFolderFieldId,
          l10n_util::GetStringUTF16(IDS_BOOKMARK_BUBBLE_FOLDER_LABEL),
          std::make_unique<RecentlyUsedFoldersComboModel>(
              bookmark_model,
              bookmark_model->GetMostRecentlyAddedUserNodeForURL(url)),
          ui::DialogModelCombobox::Params().SetCallback(
              base::BindRepeating(&BookmarkBubbleDelegate::OnComboboxAction,
                                  base::Unretained(bubble_delegate))))
      .SetInitiallyFocusedField(initially_focused_field);

  if (commerce::CanTrackPrice(product_info)) {
    bool is_price_tracked = shopping_service->IsSubscribedFromCache(
        commerce::BuildUserSubscriptionForClusterId(
            product_info->product_cluster_id.value()));
    dialog_model_builder.AddCustomField(
        std::make_unique<views::BubbleDialogModelHost::CustomView>(
            std::make_unique<PriceTrackingView>(profile, url, is_price_tracked,
                                                product_info.value()),
            views::BubbleDialogModelHost::FieldType::kControl),
        kPriceTrackingBookmarkViewElementId);

    // When a user clicks to open bookmark bubble for a product page that is
    // neither price tracked nor bookmarked, user will track the price of the
    // product, and we'll record the UKM for this event.
    // TODO(b/331277578): Move the track-by-default logic here.
    if (!is_price_tracked && !already_bookmarked) {
      commerce::metrics::RecordShoppingActionUKM(
          web_contents->GetPrimaryMainFrame()->GetPageUkmSourceId(),
          commerce::metrics::ShoppingAction::kPriceTracked);
    }
  }

  // views:: land below, there's no agnostic reference to arrow / anchors /
  // bubbles.
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_model_builder.Build(), anchor_view,
      views::BubbleBorder::TOP_RIGHT);
  bookmark_bubble_ = bubble.get();
  if (highlighted_button) {
    bubble->SetHighlightedButton(highlighted_button);
  }

  if (ShouldShowShoppingCollectionFootnote(profile, bookmark_model,
                                           bookmark_node)) {
    bubble->SetFootnoteView(
        std::make_unique<commerce::ShoppingCollectionIphView>());
  } else if (signin::ShouldShowBookmarkSignInPromo(*profile)) {
#if !BUILDFLAG(IS_CHROMEOS)
    if (!base::FeatureList::IsEnabled(syncer::kUnoPhase2FollowUp)) {
      // TODO(pbos): Consider adding model support for footnotes so that this
      // does not need to be tied to views.
      // TODO(pbos): Consider updating ::SetFootnoteView so that it can resize
      // the widget to account for it.
      bubble->SetFootnoteView(std::make_unique<BubbleSignInPromoView>(
          web_contents, signin_metrics::AccessPoint::kBookmarkBubble,
          syncer::LocalDataItemModel::DataId(bookmark_node->id()),
          ui::ButtonStyle::kDefault));

      ChromeSigninClient::
          MaybeAddUserToBookmarksBubblePromoShownSyntheticFieldTrial();
    }
#endif
  }

  bubble_delegate->SetCloseCallback(std::move(post_save_callback));

  views::Widget* const widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  widget->Show();

  bookmark_bubble_->GetBubbleFrameView()->SetProperty(
      views::kElementIdentifierKey, kBookmarkBubbleFrameViewId);

  if (star_view) {
    star_view->OnBubbleWidgetChanged(bookmark_bubble_->GetWidget());
  }
}

// static
void BookmarkBubbleView::Hide() {
  if (bookmark_bubble_) {
    bookmark_bubble_->GetWidget()->Close();
  }
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
              email: "top-chrome-desktop-ui@google.com"
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
