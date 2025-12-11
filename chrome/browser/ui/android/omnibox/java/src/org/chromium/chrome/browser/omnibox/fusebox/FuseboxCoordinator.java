// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.util.DisplayMetrics;
import android.view.LayoutInflater;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList.FuseboxAttachmentChangeListener;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.url.GURL;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/** Coordinator for the Fusebox component. */
@NullMarked
public class FuseboxCoordinator implements UrlFocusChangeListener, TemplateUrlServiceObserver {
    @IntDef({FuseboxState.DISABLED, FuseboxState.COMPACT, FuseboxState.EXPANDED})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface FuseboxState {
        int DISABLED = 0;
        int COMPACT = 1;
        int EXPANDED = 2;
    }

    private final @Nullable FuseboxViewHolder mViewHolder;
    private final @Nullable LocationBarDataProvider mLocationBarDataProvider;
    private @Nullable @BrandedColorScheme Integer mLastBrandedColorScheme;

    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final PropertyModel mModel;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final FuseboxAttachmentModelList mModelList;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private @Nullable FuseboxMediator mMediator;
    private @Nullable ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private boolean mDefaultSearchEngineIsGoogle = true;
    private TemplateUrlService mTemplateUrlService;
    private final ObservableSupplierImpl<@FuseboxState Integer> mFuseboxStateSupplier =
            new ObservableSupplierImpl<>(FuseboxState.DISABLED);
    private final ObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileObserver = this::onProfileAvailable;
    private final SnackbarManager mSnackbarManager;
    private final @Nullable ViewportRectProvider mViewportRectProvider;

    public FuseboxCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ConstraintLayout parent,
            ObservableSupplier<Profile> profileObservableSupplier,
            LocationBarDataProvider locationBarDataProvider,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            ObservableSupplierImpl<@AutocompleteRequestType Integer>
                    autocompleteRequestTypeSupplier,
            SnackbarManager snackbarManager) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mProfileSupplier = profileObservableSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mAutocompleteRequestTypeSupplier = autocompleteRequestTypeSupplier;
        mSnackbarManager = snackbarManager;
        mModelList = new FuseboxAttachmentModelList(tabModelSelectorSupplier);

        if (!OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                || parent.findViewById(R.id.fusebox_request_type) == null) {
            mViewHolder = null;
            mLocationBarDataProvider = null;
            mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);
            mViewportRectProvider = null;
            return;
        }

        mLocationBarDataProvider = locationBarDataProvider;
        templateUrlServiceSupplier.onAvailable(this::onTemplateUrlServiceAvailable);

        var contextButton = parent.findViewById(R.id.location_bar_attachments_add);
        var rectProvider = new ViewRectProvider(parent);
        rectProvider.setInsetPx(
                0,
                context.getResources()
                        .getDimensionPixelSize(R.dimen.fusebox_vertical_space_above_popup),
                0,
                0);
        var popupView = LayoutInflater.from(context).inflate(R.layout.fusebox_context_popup, null);
        mViewportRectProvider = new ViewportRectProvider(mContext);

        var popupWindowBuilder =
                new AnchoredPopupWindow.Builder(
                        mContext,
                        contextButton.getRootView(),
                        AppCompatResources.getDrawable(context, R.drawable.menu_bg_tinted),
                        () -> popupView,
                        rectProvider);
        popupWindowBuilder.setOutsideTouchable(true);
        popupWindowBuilder.setAnimateFromAnchor(true);
        popupWindowBuilder.setPreferredHorizontalOrientation(
                HorizontalOrientation.LAYOUT_DIRECTION);
        popupWindowBuilder.setViewportRectProvider(mViewportRectProvider);

        var popup = new FuseboxPopup(mContext, popupWindowBuilder.build(), popupView);
        mViewHolder = new FuseboxViewHolder(parent, popup);

        var adapter = new FuseboxAttachmentRecyclerViewAdapter(mModelList);
        mViewHolder.attachmentsView.setAdapter(adapter);

        mModel =
                new PropertyModel.Builder(FuseboxProperties.ALL_KEYS)
                        .with(FuseboxProperties.ADAPTER, adapter)
                        .with(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE, false)
                        .with(
                                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE,
                                AutocompleteRequestType.SEARCH)
                        .with(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, false)
                        // May not be correct, but the view side struggles to deal with a null here.
                        // Init with a default, and it will be corrected by the mediator before it
                        // matters.
                        .with(FuseboxProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT)
                        .with(
                                FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON,
                                OmniboxFeatures.sShowDedicatedModeButton.getValue())
                        .build();
        PropertyModelChangeProcessor.create(mModel, mViewHolder, FuseboxViewBinder::bind);
        mProfileSupplier.addObserver(mProfileObserver);
    }

    @VisibleForTesting
    void onProfileAvailable(Profile profile) {
        // Reset previous Mediator instance in case we migrate to continuous Profile observing.
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }

        mComposeBoxQueryControllerBridge = ComposeBoxQueryControllerBridge.getForProfile(profile);
        AutocompleteController.getForProfile(profile)
                .setComposeboxQueryControllerBridge(mComposeBoxQueryControllerBridge);
        if (mComposeBoxQueryControllerBridge == null) return;

        // Set the bridge for the model list to enable tight coupling
        mModelList.setComposeBoxQueryControllerBridge(mComposeBoxQueryControllerBridge);

        mModel.set(
                FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE,
                mComposeBoxQueryControllerBridge.isCreateImagesEligible()
                        && (OmniboxFeatures.sShowImageGenerationButtonInIncognito.getValue()
                                || !profile.isIncognitoBranded()));
        mMediator =
                new FuseboxMediator(
                        mContext,
                        profile,
                        mWindowAndroid,
                        mModel,
                        assumeNonNull(mViewHolder),
                        mModelList,
                        mAutocompleteRequestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mComposeBoxQueryControllerBridge,
                        mFuseboxStateSupplier,
                        mSnackbarManager,
                        () -> mTemplateUrlService);
        if (mLastBrandedColorScheme != null) {
            mMediator.updateVisualsForState(mLastBrandedColorScheme);
        }
        mModelList.setAttachmentUploadFailedListener(mMediator::onAttachmentUploadFailed);
    }

    public void destroy() {
        mProfileSupplier.removeObserver(mProfileObserver);
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(this);
        }
        // Clear the model list bridge reference to prevent further operations
        mModelList.setComposeBoxQueryControllerBridge(null);
        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.destroy();
            mComposeBoxQueryControllerBridge = null;
        }
        if (mViewportRectProvider != null) {
            mViewportRectProvider.destroy();
        }
    }

    /** Apply a variant of the branded color scheme to Fusebox UI elements */
    public void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        if (mMediator == null) return;
        mLastBrandedColorScheme = brandedColorScheme;
        mMediator.updateVisualsForState(brandedColorScheme);
    }

    public void onAiModeActivatedFromNtp() {
        if (mMediator == null) return;
        mMediator.activateAiMode(AiModeActivationSource.NTP_BUTTON);
    }

    /** Called when the URL focus changes. */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (mMediator == null
                || mLocationBarDataProvider == null
                || !mDefaultSearchEngineIsGoogle) {
            return;
        }

        int pageClass = mLocationBarDataProvider.getPageClassification(/* prefetch= */ false);

        boolean isSupportedPageClass =
                switch (pageClass) {
                    case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE,
                            PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE,
                            PageClassification.OTHER_VALUE ->
                            true;
                    default -> false;
                };

        boolean isChangeable = hasFocus && isSupportedPageClass;
        mMediator.setAutocompleteRequestTypeChangeable(isChangeable);
        mMediator.setToolbarVisible(isChangeable);
        if (isChangeable) {
            FuseboxMetrics.notifyOmniboxSessionStarted();
        }
    }

    // TemplateUrlServiceObserver
    @Override
    public void onTemplateURLServiceChanged() {
        // TODO(https://crbug.com/465744465): Consider hard resetting mediator instead.
        boolean isDseGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (isDseGoogle == mDefaultSearchEngineIsGoogle) return;

        mDefaultSearchEngineIsGoogle = isDseGoogle;
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        mDefaultSearchEngineIsGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (mMediator != null && !mDefaultSearchEngineIsGoogle) {
            mMediator.setToolbarVisible(false);
        }
    }

    /**
     * @return An {@link ObservableSupplier} that notifies observers when the autocomplete request
     *     type changes.
     */
    public ObservableSupplier<@AutocompleteRequestType Integer>
            getAutocompleteRequestTypeSupplier() {
        return mAutocompleteRequestTypeSupplier;
    }

    /** Returns the URL associated with the current AIM session. */
    public GURL getAimUrl(GURL url) {
        if (mMediator == null) return GURL.emptyGURL();
        return mMediator.getAimUrl(url);
    }

    /** Returns the URL associated with the current image generation session. */
    public GURL getImageGenerationUrl(GURL url) {
        if (mMediator == null) return GURL.emptyGURL();
        return mMediator.getImageGenerationUrl(url);
    }

    public PropertyModel getModelForTesting() {
        return mModel;
    }

    @Nullable FuseboxViewHolder getViewHolderForTesting() {
        return mViewHolder;
    }

    void setMediatorForTesting(FuseboxMediator mediator) {
        mMediator = mediator;
    }

    @Nullable FuseboxMediator getMediatorForTesting() {
        return mMediator;
    }

    @Initializer
    private void onTemplateUrlServiceAvailable(TemplateUrlService templateUrlService) {
        mTemplateUrlService = templateUrlService;
        mTemplateUrlService.addObserver(this);
        onTemplateURLServiceChanged();
    }

    /**
     * Called when fusebox text wrapping changes.
     *
     * @param isWrapping true if text is wrapping (should show expanded UI), false for compact UI
     */
    public void onFuseboxTextWrappingChanged(boolean isWrapping) {
        // We only care about url bar wrapping state when compact variant is enabled. Guard against
        // entering compact mode when the variant is disabled by returning early.
        if (mMediator == null || !OmniboxFeatures.sCompactFusebox.getValue()) return;
        mMediator.setUseCompactUi(
                !isWrapping
                        && mAutocompleteRequestTypeSupplier.get()
                                == AutocompleteRequestType.SEARCH);
    }

    /**
     * Whether the given mode allows "conventional" fulfillment of a valid typed url, i.e.
     * navigating to that url directly. As an example of where this might return false: if if the
     * user types www.foo.com and presses enter with this mode active, they will be taken to some
     * DSE-specific landing page where www.foo.com is the input, not directly to foo.com *
     */
    public static boolean isConventionalFulfillmentType(@AutocompleteRequestType int mode) {
        return mode == AutocompleteRequestType.SEARCH;
    }

    public void notifyOmniboxSessionEnded(boolean userDidNavigate) {
        FuseboxMetrics.notifyOmniboxSessionEnded(
                userDidNavigate, mAutocompleteRequestTypeSupplier.get());
    }

    /**
     * Registers a callback notified when the compactness of the fusebox changes. This callback will
     * only fire if the compact mode variant is enabled and the compactness state changes.
     */
    public ObservableSupplier<@FuseboxState Integer> getFuseboxStateSupplier() {
        return mFuseboxStateSupplier;
    }

    /** Registers the listener notified whenever attachments list is changed. */
    public void addAttachmentChangeListener(FuseboxAttachmentChangeListener listener) {
        mModelList.addAttachmentChangeListener(listener);
    }

    /** Unregisters the listener from being notified that attachments list has been changed. */
    public void removeAttachmentChangeListener(FuseboxAttachmentChangeListener listener) {
        mModelList.removeAttachmentChangeListener(listener);
    }

    /** Returns the number of attachments in the Fusebox Attachments list. */
    public int getAttachmentsCount() {
        return mModelList.size();
    }

    /**
     * Provider of the viewport for the fusebox popup window. This implementation treats the entire
     * window as available, ignoring e.g. ime insets which can reduce the available height to a very
     * small quantity using PopupWindow's default viewport rect.
     */
    static class ViewportRectProvider extends RectProvider implements ComponentCallbacks {
        private final Context mContext;

        public ViewportRectProvider(Context context) {
            mContext = context;
            mContext.registerComponentCallbacks(this);
            updateRect();
        }

        @Override
        public void onConfigurationChanged(@NonNull Configuration configuration) {
            updateRect();
        }

        private void updateRect() {
            DisplayMetrics displayMetrics = mContext.getResources().getDisplayMetrics();
            mRect.set(0, 0, displayMetrics.widthPixels, displayMetrics.heightPixels);
        }

        @Override
        public void onLowMemory() {}

        public void destroy() {
            mContext.unregisterComponentCallbacks(this);
        }
    }
}
