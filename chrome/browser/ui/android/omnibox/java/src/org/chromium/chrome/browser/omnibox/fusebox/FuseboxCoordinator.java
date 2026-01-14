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
import androidx.appcompat.content.res.AppCompatResources;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList.FuseboxAttachmentChangeListener;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
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
public class FuseboxCoordinator implements TemplateUrlServiceObserver {
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
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final PropertyModel mModel;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final FuseboxAttachmentModelList mModelList;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private @Nullable FuseboxMediator mMediator;

    private boolean mDefaultSearchEngineIsGoogle = true;
    private TemplateUrlService mTemplateUrlService;
    private final ObservableSupplierImpl<@FuseboxState Integer> mFuseboxStateSupplier =
            new ObservableSupplierImpl<>(FuseboxState.DISABLED);

    private final SnackbarManager mSnackbarManager;
    private final @Nullable ViewportRectProvider mViewportRectProvider;

    public FuseboxCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ConstraintLayout parent,
            LocationBarDataProvider locationBarDataProvider,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            ObservableSupplierImpl<@AutocompleteRequestType Integer>
                    autocompleteRequestTypeSupplier,
            SnackbarManager snackbarManager) {
        mContext = context;
        mWindowAndroid = windowAndroid;
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

        var adapter = mModelList.getAdapter();
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

        mMediator =
                new FuseboxMediator(
                        mContext,
                        mWindowAndroid,
                        mModel,
                        assumeNonNull(mViewHolder),
                        mModelList,
                        mAutocompleteRequestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mFuseboxStateSupplier,
                        mSnackbarManager);

        PropertyModelChangeProcessor.create(mModel, mViewHolder, FuseboxViewBinder::bind);
    }

    public void destroy() {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(this);
        }
        // Clear the model list bridge reference to prevent further operations
        mModelList.setComposeBoxQueryControllerBridge(null);
        // Session cleanup is handled by setInputSession(null)
        setInputSession(null);
        if (mViewportRectProvider != null) {
            mViewportRectProvider.destroy();
        }
    }

    @EnsuresNonNullIf("mMediator")
    private boolean isInInputSession() {
        return mMediator != null && mMediator.isInInputSession();
    }

    /** Apply a variant of the branded color scheme to Fusebox UI elements */
    public void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        if (mMediator == null) return;
        mMediator.updateVisualsForState(brandedColorScheme);
    }

    public void onAiModeActivatedFromNtp() {
        if (!isInInputSession()) return;
        mMediator.activateAiMode(AiModeActivationSource.NTP_BUTTON);
    }

    /**
     * Sets the session state for this FuseboxCoordinator. This should be called by the
     * LocationBarMediator when a new session is created.
     *
     * @param inputSession The session state containing Profile and ComposeBoxQueryControllerBridge,
     *     or null to clear the current session.
     */
    public void setInputSession(@Nullable FuseboxInputSession inputSession) {
        // Mediator may not be created if required views are not available,
        // or if the corresponding feature flag is disabled.
        // See the Constructor early return case.
        if (mMediator == null) return;

        mMediator.setInputSession(inputSession);

        boolean isSupportedPageClass = false;
        if (mLocationBarDataProvider != null) {
            int pageClass = mLocationBarDataProvider.getPageClassification(/* prefetch= */ false);
            isSupportedPageClass =
                    switch (pageClass) {
                        // LINT.IfChange(FuseboxSupportedPageClassifications)
                        case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE,
                                PageClassification
                                        .SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE,
                                PageClassification.OTHER_VALUE ->
                                true;
                        // LINT.ThenChange(/components/omnibox/browser/android/java/src/org/chromium/components/omnibox/AutocompleteInput.java:FuseboxSupportedPageClassifications)
                        default -> false;
                    };
        }

        // Stop here if Fusebox should not show.
        if (!OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                || !isInInputSession()
                || !isSupportedPageClass
                || !mDefaultSearchEngineIsGoogle) {
            mMediator.setAutocompleteRequestTypeChangeable(false);
            mMediator.setToolbarVisible(false);
            mModelList.setComposeBoxQueryControllerBridge(null);
            return;
        }

        // Checked by isInInputSession; enforced by NullAway
        assert inputSession != null && inputSession.composeBoxController != null;

        mModelList.setComposeBoxQueryControllerBridge(inputSession.composeBoxController);

        // Set the bridge for the model list to enable tight coupling
        // TODO(...): this doesn't belong here.
        mModel.set(
                FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE,
                inputSession.composeBoxController.isCreateImagesEligible()
                        && (OmniboxFeatures.sShowImageGenerationButtonInIncognito.getValue()
                                || !inputSession.profile.isIncognitoBranded()));

        mModelList.setAttachmentUploadFailedListener(mMediator::onAttachmentUploadFailed);
        mMediator.setAutocompleteRequestTypeChangeable(true);
        mMediator.setToolbarVisible(true);
        FuseboxMetrics.notifyOmniboxSessionStarted();
    }

    // TemplateUrlServiceObserver
    @Override
    public void onTemplateURLServiceChanged() {
        boolean isDseGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (isDseGoogle == mDefaultSearchEngineIsGoogle) return;

        mDefaultSearchEngineIsGoogle = isDseGoogle;
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);
        mDefaultSearchEngineIsGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (isInInputSession() && !mDefaultSearchEngineIsGoogle) {
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

    /**
     * Retrieves the URL for the current AIM session.
     *
     * <p>This is an asynchronous operation. The resulting URL is passed to the provided callback.
     * If no session is active, the callback receives an empty GURL.
     *
     * @param url The base URL to be used for the AIM session.
     * @param callback The callback to receive the AIM session URL.
     */
    public void getAimUrl(GURL url, Callback<GURL> callback) {
        if (!isInInputSession()) {
            callback.onResult(GURL.emptyGURL());
            return;
        }
        mMediator.getAimUrl(url, callback);
    }

    /**
     * Retrieves the URL for the current image generation session.
     *
     * <p>This is an asynchronous operation. The resulting URL is passed to the provided callback.
     * If no session is active, the callback receives an empty GURL.
     *
     * @param url The base URL to be used for the image generation session.
     * @param callback The callback to receive the image generation session URL.
     */
    public void getImageGenerationUrl(GURL url, Callback<GURL> callback) {
        if (!isInInputSession()) {
            callback.onResult(GURL.emptyGURL());
            return;
        }
        mMediator.getImageGenerationUrl(url, callback);
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
     * Handles changes in the text wrapping state of the Fusebox.
     *
     * <p>This method is called when the omnibox text wraps or unwraps. It adjusts the Fusebox UI to
     * be compact when text is not wrapping and expanded when it is. This behavior is only active
     * when the compact Fusebox feature is enabled.
     *
     * @param isWrapping {@code true} if the text is wrapping, {@code false} otherwise.
     */
    public void onFuseboxTextWrappingChanged(boolean isWrapping) {
        // We only care about url bar wrapping state when compact variant is enabled. Guard against
        // entering compact mode when the variant is disabled by returning early.
        if (!isInInputSession() || !OmniboxFeatures.sCompactFusebox.getValue()) return;
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

    /**
     * Notifies that the Fusebox session has ended and records relevant metrics.
     *
     * @param userDidNavigate Whether the user navigated to a URL as a result of the session.
     */
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

    /**
     * Registers a listener to be notified of changes to the attachments list.
     *
     * @param listener The listener to add.
     */
    public void addAttachmentChangeListener(FuseboxAttachmentChangeListener listener) {
        mModelList.addAttachmentChangeListener(listener);
    }

    /**
     * Unregisters a listener from attachment list change notifications.
     *
     * @param listener The listener to remove.
     */
    public void removeAttachmentChangeListener(FuseboxAttachmentChangeListener listener) {
        mModelList.removeAttachmentChangeListener(listener);
    }

    /**
     * Returns the current number of attachments in the Fusebox.
     *
     * @return The number of attachments.
     */
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
        public void onConfigurationChanged(Configuration configuration) {
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
