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
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.Callback;
import org.chromium.base.DeviceInfo;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList.FuseboxAttachmentChangeListener;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
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
    private @Nullable @BrandedColorScheme Integer mLastBrandedColorScheme;
    private final PropertyModel mModel;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final FuseboxAttachmentModelList mModelList;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private @Nullable FuseboxMediator mMediator;
    private @Nullable AutocompleteInput mInput;
    private boolean mDefaultSearchEngineIsGoogle = true;
    private TemplateUrlService mTemplateUrlService;
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Callback<Profile> mProfileObserver = this::onProfileAvailable;
    private final SnackbarManager mSnackbarManager;
    private final @Nullable ViewportRectProvider mViewportRectProvider;

    /**
     * Creates a new instance of {@link FuseboxCoordinator}.
     *
     * @param context The context to create views and retrieve resources.
     * @param windowAndroid The window to attach views to.
     * @param parent The parent view to attach the fusebox to.
     * @param profileObservableSupplier The supplier of the current profile.
     * @param tabModelSelectorSupplier The supplier of the tab model selector.
     * @param templateUrlServiceSupplier The supplier of the template URL service.
     * @param snackbarManager The snackbar manager to show messages.
     */
    public FuseboxCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ConstraintLayout parent,
            MonotonicObservableSupplier<Profile> profileObservableSupplier,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            SnackbarManager snackbarManager) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mProfileSupplier = profileObservableSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mSnackbarManager = snackbarManager;
        mModelList = new FuseboxAttachmentModelList();

        if (!OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                || parent.findViewById(R.id.fusebox_request_type) == null) {
            mViewHolder = null;
            mModel = new PropertyModel(FuseboxProperties.ALL_KEYS);
            mViewportRectProvider = null;
            return;
        }
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
                        // May not be correct, but the view side struggles to deal with a null here.
                        // Init with a default, and it will be corrected by the mediator before it
                        // matters.
                        .with(FuseboxProperties.COLOR_SCHEME, BrandedColorScheme.APP_DEFAULT)
                        .with(
                                FuseboxProperties.SHOW_DEDICATED_MODE_BUTTON,
                                OmniboxFeatures.sShowDedicatedModeButton.getValue())
                        .build();
        PropertyModelChangeProcessor.create(mModel, mViewHolder, FuseboxViewBinder::bind);
        mProfileSupplier.addSyncObserverAndPostIfNonNull(mProfileObserver);
    }

    @VisibleForTesting
    void onProfileAvailable(Profile profile) {
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }

        mMediator =
                new FuseboxMediator(
                        mContext,
                        profile,
                        mWindowAndroid,
                        mModel,
                        assumeNonNull(mViewHolder),
                        mTabModelSelectorSupplier,
                        mFuseboxStateSupplier,
                        mSnackbarManager);
        if (mLastBrandedColorScheme != null) {
            mMediator.updateVisualsForState(mLastBrandedColorScheme);
        }
    }

    public void destroy() {
        endInput();
        mProfileSupplier.removeObserver(mProfileObserver);
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(this);
        }
        mModelList.destroy();
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

    /**
     * Called when the user begins / resumes interacting with the Omnibox.
     *
     * <p>This method evaluates the current context to decide whether to activate the Fusebox UI.
     * Fusebox will not be activated if the feature is not initialized, the current page is not
     * supported, or if the default search engine is not Google.
     *
     * @param session The session state for this session. A new session may be applied without going
     *     through the endInput() (valid -> valid). This is the case for tab switching.
     */
    public void beginInput(FuseboxSessionState session) {
        // Abort early if there is no composebox.
        if (session.getComposeboxQueryControllerBridge() == null) return;

        // We can't do inclusive check due to missing `isPhone()` case in `DeviceInfo`.
        // Additionally these values may change at runtime, e.g. if the user starts Chrome on phone
        // and moves to Android Auto.
        boolean isSupportedDeviceType =
                !DeviceInfo.isAutomotive() && !DeviceInfo.isXr() && !DeviceInfo.isTV();
        boolean isSupportedPageClass =
                switch (session.getAutocompleteInput().getRawPageClassification()) {
                    // LINT.IfChange(FuseboxSupportedPageClassifications)
                    case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS_VALUE,
                            PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT_VALUE,
                            PageClassification.OTHER_VALUE ->
                            true;
                    // LINT.ThenChange(/components/omnibox/browser/android/java/src/org/chromium/components/omnibox/AutocompleteInput.java:FuseboxSupportedPageClassifications)
                    default -> false;
                };

        // Terminate any current input to re-set session and re-install observers.
        // This should ideally be an assert ensuring that we don't begin a new input while the old
        // one is still active; will turn to an assert separately in case this scenario happens.
        if (mMediator == null
                || !isSupportedDeviceType
                || !isSupportedPageClass
                || !mDefaultSearchEngineIsGoogle) {
            endInput();
            return;
        }

        // TODO(crbug.com/474616308): move to FuseboxSessionState.
        AutocompleteController.getForProfile(assumeNonNull(session.getProfile()))
                .setComposeboxQueryControllerBridge(session.getComposeboxQueryControllerBridge());

        mInput = session.getAutocompleteInput();
        mMediator.beginInput(session);
        FuseboxMetrics.notifyOmniboxSessionStarted();
    }

    /** Called when the user stops interacting with the Omnibox. */
    public void endInput() {
        if (mMediator != null) {
            mMediator.endInput();
        }
        mInput = null;
    }

    // TemplateUrlServiceObserver
    @Override
    public void onTemplateURLServiceChanged() {
        boolean isDseGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (isDseGoogle == mDefaultSearchEngineIsGoogle) return;
        mDefaultSearchEngineIsGoogle = isDseGoogle;

        if (mInput != null && !mDefaultSearchEngineIsGoogle) {
            mInput.setRequestType(AutocompleteRequestType.SEARCH);
            assumeNonNull(mMediator).setToolbarVisible(false);
        }
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
        if (mInput == null || !OmniboxFeatures.sCompactFusebox.getValue()) return;
        assumeNonNull(mMediator)
                .setUseCompactUi(
                        !isWrapping && mInput.getRequestType() == AutocompleteRequestType.SEARCH);
    }

    public void notifyOmniboxSessionEnded(boolean userDidNavigate) {
        // Skip cases where session should not be recorded (e.g. unsupported page class).
        if (mInput == null) return;
        FuseboxMetrics.notifyOmniboxSessionEnded(userDidNavigate, mInput.getRequestType());
    }

    /**
     * Registers a callback notified when the compactness of the fusebox changes. This callback will
     * only fire if the compact mode variant is enabled and the compactness state changes.
     */
    public NonNullObservableSupplier<@FuseboxState Integer> getFuseboxStateSupplier() {
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
