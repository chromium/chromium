// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.graphics.Rect;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;
import android.widget.PopupWindow;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsCompat.Type;
import androidx.window.layout.WindowMetricsCalculator;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager.ScrimClient;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteInput.AutocompleteState;
import org.chromium.components.omnibox.AutocompleteInput.DisplayState;
import org.chromium.components.omnibox.OmniboxCapabilities;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.AsyncLayoutInflater;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.insets.InsetObserver;
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
import java.util.function.Supplier;

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

    @IntDef({FuseboxLayoutMode.TOOLBAR, FuseboxLayoutMode.SUGGESTIONS_POPOVER})
    @Retention(RetentionPolicy.SOURCE)
    @Target(ElementType.TYPE_USE)
    public @interface FuseboxLayoutMode {
        // The Fusebox UI is embedded in the toolbar, separate from associated suggestions.
        int TOOLBAR = 1;
        // The Fusebox UI is intermingled with associated suggestions in a single popover window.
        int SUGGESTIONS_POPOVER = 2;
    }

    @IntDef({PopupState.HIDDEN, PopupState.FLOATING, PopupState.BOTTOM})
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.TYPE_USE})
    public @interface PopupState {
        int HIDDEN = 0;
        int FLOATING = 1;
        int BOTTOM = 2;
    }

    private @Nullable FuseboxViewHolder mViewHolder;
    private @Nullable PropertyModel mModel;
    private final Activity mActivity;
    private final WindowAndroid mWindowAndroid;
    private final ConstraintLayout mParent;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private @Nullable AutocompleteInput mInput;
    private boolean mDefaultSearchEngineIsGoogle = true;
    private boolean mDeferredInitialized;
    private @Nullable FuseboxSessionState mPendingSession;
    private TemplateUrlService mTemplateUrlService;
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier =
            ObservableSuppliers.createNonNull(FuseboxState.DISABLED);
    private final SettableNonNullObservableSupplier<@FuseboxLayoutMode Integer>
            mFuseboxLayoutModeSupplier =
                    ObservableSuppliers.createNonNull(FuseboxLayoutMode.TOOLBAR);
    private final SettableNonNullObservableSupplier<@PopupState Integer> mPopupStateSupplier =
            ObservableSuppliers.createNonNull(PopupState.HIDDEN);
    private final SettableNonNullObservableSupplier<Boolean> mHasAttachmentsSupplier =
            ObservableSuppliers.createNonNull(/* initialValue= */ false);
    private final SnackbarManager mSnackbarManager;
    private @Nullable ViewportRectProvider mViewportRectProvider;
    private @Nullable FuseboxMetrics mMetrics;
    private @Nullable BottomSheetRectProvider mBottomSheetRectProvider;
    private final Supplier<@Nullable View> mScrimAnchorViewSupplier;
    private final ScrimManager mScrimManager;
    private final BackPressManager mBackPressManager;

    // Mediator is scoped to a particular profile. Can reuse as long as the profile does not change.
    private @Nullable FuseboxMediator mMediator;
    private @Nullable @BrandedColorScheme Integer mLastBrandedColorScheme;
    private boolean mDestroyed;
    private @Nullable Callback<Boolean> mOnInteractionCompletedCallback;
    private @Nullable Runnable mOnFirstPickerInteractionCanceledCallback;
    private final boolean mIsForcedPhoneStyleOmnibox;
    private final OmniboxResourceProvider mResourceProvider;

    /**
     * Creates a new instance of {@link FuseboxCoordinator}.
     *
     * @param context The context to create views and retrieve resources.
     * @param windowAndroid The window to attach views to.
     * @param parent The parent view to attach the fusebox to.
     * @param resourceProvider The resource provider for retrieving resources.
     * @param tabModelSelectorSupplier The supplier of the tab model selector.
     * @param templateUrlServiceSupplier The supplier of the template URL service.
     * @param snackbarManager The snackbar manager to show messages.
     * @param scrimAnchorViewSupplier Supplier for the view to anchor the scrim to.
     * @param backPressManager The back press manager to register the back press handler.
     * @param isForcedPhoneStyleOmnibox Whether to force phone-style Omnibox layout.
     */
    public FuseboxCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ConstraintLayout parent,
            OmniboxResourceProvider resourceProvider,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            SnackbarManager snackbarManager,
            Supplier<@Nullable View> scrimAnchorViewSupplier,
            BackPressManager backPressManager,
            boolean isForcedPhoneStyleOmnibox) {
        mActivity = assumeNonNull(ContextUtils.activityFromContext(context));
        mWindowAndroid = windowAndroid;
        mParent = parent;
        mResourceProvider = resourceProvider;
        ViewGroup contentView = mActivity.findViewById(android.R.id.content);
        // TODO(crbug.com/509962912): Consider using RootUiCoordinator's ScrimManager.
        mScrimManager = new ScrimManager(context, contentView, ScrimClient.FUSEBOX_POPUP);
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mSnackbarManager = snackbarManager;
        mScrimAnchorViewSupplier = scrimAnchorViewSupplier;
        mIsForcedPhoneStyleOmnibox = isForcedPhoneStyleOmnibox;
        mFuseboxLayoutModeSupplier.set(getFuseboxLayoutMode());
        mBackPressManager = backPressManager;

        if (!OmniboxFeatures.isMultimodalInputEnabled(context)
                || parent.findViewById(R.id.fusebox_request_type) == null) {
            mDeferredInitialized = true;
            return;
        }

        templateUrlServiceSupplier.onAvailable(this::onTemplateUrlServiceAvailable);
    }

    private void ensureDeferredInitialized() {
        if (mDeferredInitialized) return;
        mDeferredInitialized = true;

        mModel =
                new PropertyModel.Builder(FuseboxProperties.ALL_KEYS)
                        .with(FuseboxProperties.FUSEBOX_LAYOUT_MODE, getFuseboxLayoutMode())
                        .with(
                                FuseboxProperties.POPUP_IS_BOTTOM_SHEET,
                                OmniboxFeatures.shouldShowBottomSheetPopup())
                        .build();

        new AsyncLayoutInflater(mActivity)
                .inflate(R.layout.fusebox_context_popup, this::finishDeferredInitialization);
    }

    private void finishDeferredInitialization(View popupView) {
        if (mDestroyed) return;

        ViewRectProvider floatingViewRectProvider;
        if (getFuseboxLayoutMode() == FuseboxLayoutMode.SUGGESTIONS_POPOVER) {
            // Popover never uses StatusView to show plus button, so this is safe here.
            View plusButton = mParent.findViewById(R.id.fusebox_plus_button);
            floatingViewRectProvider = new ViewRectProvider(plusButton);
        } else {
            // Instead of anchoring on the plus button or status view, anchor on the parent and then
            // shift down slightly. This gives better behavior on small screens.
            Resources res = mActivity.getResources();
            floatingViewRectProvider = new ViewRectProvider(mParent);
            floatingViewRectProvider.setInsetPx(
                    /* left= */ 0,
                    res.getDimensionPixelSize(R.dimen.fusebox_vertical_space_above_popup),
                    /* right= */ 0,
                    /* bottom= */ 0);
        }
        mBottomSheetRectProvider = new BottomSheetRectProvider(mActivity, mParent);

        DynamicRectProvider dynamicRectProvider =
                new DynamicRectProvider(floatingViewRectProvider, mBottomSheetRectProvider);
        mViewportRectProvider =
                new ViewportRectProvider(mActivity, mWindowAndroid.getInsetObserver(), mParent);

        var popupWindowBuilder =
                new AnchoredPopupWindow.Builder(
                                mActivity,
                                mParent.getRootView(),
                                mResourceProvider.getPopupBackgroundDrawable(),
                                () -> popupView,
                                dynamicRectProvider)
                        .addOnDismissListener(this::onContextPopupDismissed)
                        .setOutsideTouchable(/* touchable= */ true)
                        .setFocusable(/* focusable= */ true)
                        .setInputMethodMode(PopupWindow.INPUT_METHOD_NOT_NEEDED)
                        .setAnimateFromAnchor(/* animateFromAnchor= */ true)
                        .setPreferredHorizontalOrientation(HorizontalOrientation.LAYOUT_DIRECTION)
                        .setViewportRectProvider(mViewportRectProvider)
                        .setHorizontalOverlapAnchor(/* overlap= */ true)
                        .setVerticalOverlapAnchor(/* overlap= */ true)
                        .setAllowNonTouchableSize(/* allow= */ true);

        FuseboxPopup popup =
                new FuseboxPopup(
                        mActivity,
                        mWindowAndroid,
                        popupWindowBuilder.build(),
                        popupView,
                        dynamicRectProvider,
                        OmniboxFeatures.shouldShowBottomSheetPopup());

        mViewHolder = new FuseboxViewHolder(mParent, popup);

        if (mPendingSession != null) {
            beginInput(mPendingSession);
            mPendingSession = null;
        }

        ThreadUtils.postOnUiThread(
                () -> {
                    if (mDestroyed || mModel == null || mViewHolder == null) return;

                    PropertyModelChangeProcessor.create(
                            mModel, mViewHolder, new FuseboxViewBinder(mResourceProvider)::bind);
                });
    }

    @EnsuresNonNull("mMediator")
    private void ensureMediatorCreated() {
        if (mMediator != null) return;

        mMediator =
                new FuseboxMediator(
                        mActivity,
                        mWindowAndroid,
                        assumeNonNull(mModel),
                        assumeNonNull(mViewHolder),
                        mResourceProvider,
                        mTabModelSelectorSupplier,
                        mFuseboxStateSupplier,
                        mPopupStateSupplier,
                        mSnackbarManager,
                        mScrimManager,
                        mScrimAnchorViewSupplier,
                        mBackPressManager,
                        mOnFirstPickerInteractionCanceledCallback,
                        mHasAttachmentsSupplier);
        if (mLastBrandedColorScheme != null) {
            mMediator.updateVisualsForState(mLastBrandedColorScheme);
        }
    }

    public void destroy() {
        mDestroyed = true;
        endInput();
        if (mMediator != null) {
            mMediator.destroy();
            mMediator = null;
        }
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(this);
        }
        if (mViewportRectProvider != null) {
            mViewportRectProvider.destroy();
        }
        if (mBottomSheetRectProvider != null) {
            mBottomSheetRectProvider.destroy();
        }
        if (mViewHolder != null) {
            mViewHolder.popup.destroy();
        }
        mScrimManager.destroy();
    }

    /** Apply a variant of the branded color scheme to Fusebox UI elements */
    public void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        mLastBrandedColorScheme = brandedColorScheme;
        if (mMediator != null) {
            mMediator.updateVisualsForState(brandedColorScheme);
        }
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
        var composeBox = session.getComposeboxQueryControllerBridge();
        if (composeBox == null) return;

        // We can't do inclusive check due to missing `isPhone()` case in `DeviceInfo`.
        // Additionally these values may change at runtime, e.g. if the user starts Chrome on phone
        // and moves to Android Auto.
        boolean isSupportedDeviceType = OmniboxCapabilities.isFuseboxSupportedDeviceType();
        boolean isSupportedPageClass =
                switch (session.getAutocompleteInput().getRawPageClassification()) {
                    // LINT.IfChange(FuseboxSupportedPageClassifications)
                    case PageClassification.INSTANT_NTP_WITH_OMNIBOX_AS_STARTING_FOCUS,
                            PageClassification.SEARCH_RESULT_PAGE_NO_SEARCH_TERM_REPLACEMENT,
                            PageClassification.OTHER ->
                            true;
                    // LINT.ThenChange(/components/omnibox/browser/android/java/src/org/chromium/components/omnibox/AutocompleteInput.java:FuseboxSupportedPageClassifications)
                    default -> false;
                };

        // Terminate any current input to re-set session and re-install observers.
        // This should ideally be an assert ensuring that we don't begin a new input while the old
        // one is still active; will turn to an assert separately in case this scenario happens.
        if (!composeBox.isFuseboxEligible()
                || !isSupportedDeviceType
                || !isSupportedPageClass
                || !mDefaultSearchEngineIsGoogle) {
            endInput();
            return;
        }

        if (mViewHolder == null) {
            // - If mDeferredInitialized is false - this is the first time we run beginInput and we
            //   need to make sure the UI is built.
            // - If mDeferredInitialized is true, but mViewHolder is null - then we already
            //   determined that the user or scenario is not eligible (e.g. feature flag disabled).
            if (!mDeferredInitialized) {
                mPendingSession = session;
                // A number of UI components, like the StatusView, depend on the FuseboxState. The
                // FuseboxMediator will eventually control the FuseboxState, but it's is not yet
                // created here. So, we do this initial assignment to ensure that these UI
                // components don't show any bad intermediate states in the meantime.
                mFuseboxStateSupplier.set(
                        session.getAutocompleteInput().getAutocompleteState()
                                                == AutocompleteState.STANDBY_NO_FOCUS
                                        || session.getAutocompleteInput().getDisplayState()
                                                == DisplayState.DRAFTING_NO_FOCUS
                                ? FuseboxState.DISABLED
                                : FuseboxState.COMPACT);
                ensureDeferredInitialized();
            }
            return;
        }

        ensureMediatorCreated();

        mInput = session.getAutocompleteInput();
        mMetrics = session.getMetrics();
        mMediator.beginInput(session);
        if (mMetrics != null) {
            mMetrics.notifyOmniboxSessionStarted();
        }
    }

    /** Called when the user stops interacting with the Omnibox. */
    public void endInput() {
        if (mMediator != null) {
            mMediator.endInput();
        }
        mInput = null;
        mMetrics = null;
        mPendingSession = null;
    }

    /**
     * Handle a key event by activating it or changing the currently keyboard-selected view; returns
     * true if a view was selected or activated.
     */
    public boolean handleKeyEvent(int keyCode, KeyEvent event) {
        return mMediator != null && mMediator.handleKeyEvent(keyCode, event);
    }

    /** Set the first attachment as selected. Does nothing if there are not attachments. */
    public void selectFirstAttachment() {
        if (mMediator == null) return;
        mMediator.selectFirstAttachment();
    }

    /** Set the last attachment as selected. Does nothing if there are not attachments. */
    public void selectLastAttachment() {
        if (mMediator == null) return;
        mMediator.selectLastAttachment();
    }

    // TemplateUrlServiceObserver
    @Override
    public void onTemplateURLServiceChanged() {
        boolean isDseGoogle = mTemplateUrlService.isDefaultSearchEngineGoogle();
        if (isDseGoogle == mDefaultSearchEngineIsGoogle) return;
        mDefaultSearchEngineIsGoogle = isDseGoogle;

        if (mInput != null && !mDefaultSearchEngineIsGoogle) {
            resetToSearchMode();
            endInput();
        }
    }

    public @Nullable PropertyModel getModelForTesting() {
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

    @VisibleForTesting
    void onContextPopupDismissed() {
        if (mViewHolder == null || mViewHolder.plusButton == null) return;
        boolean popupItemSelected = mMediator != null && mMediator.wasPopupItemSelected();
        if (!popupItemSelected) {
            mViewHolder.plusButton.requestFocus();
            mViewHolder.plusButton.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
        }
        if (mOnInteractionCompletedCallback != null) {
            mOnInteractionCompletedCallback.onResult(popupItemSelected);
        }
    }

    @Initializer
    private void onTemplateUrlServiceAvailable(TemplateUrlService templateUrlService) {
        mTemplateUrlService = templateUrlService;
        mTemplateUrlService.addObserver(this);
        onTemplateURLServiceChanged();
    }

    /**
     * @param isTextWrapping Whether the text is wrapping or not.
     */
    public void onFuseboxTextWrappingChanged(boolean isTextWrapping) {
        if (mMediator != null) {
            mMediator.setIsTextWrapping(isTextWrapping);
        }
    }

    public void notifyOmniboxSessionEnded(boolean userDidNavigate) {
        // Skip cases where session should not be recorded (e.g. unsupported page class).
        if (mInput == null || mMetrics == null) return;
        mMetrics.notifyOmniboxSessionEnded(
                userDidNavigate, mInput.getRequestType(), mInput.getModelMode());
    }

    /** Resets the current input session back to search mode. */
    public void resetToSearchMode() {
        if (mMediator != null) {
            mMediator.activateSearchMode();
        }
    }

    /** Toggles the attachments, tools, and models menu. */
    public void plusButtonClicked() {
        if (mMediator != null) {
            mMediator.onPlusButtonClicked();
        }
    }

    /**
     * Registers a callback notified when the compactness of the fusebox changes. This callback will
     * only fire if the compact mode variant is enabled and the compactness state changes.
     */
    public NonNullObservableSupplier<@FuseboxState Integer> getFuseboxStateSupplier() {
        return mFuseboxStateSupplier;
    }

    /** Registers a callback notified when the layout mode of the fusebox changes. */
    public NonNullObservableSupplier<@FuseboxLayoutMode Integer> getFuseboxLayoutModeSupplier() {
        return mFuseboxLayoutModeSupplier;
    }

    /** Registers a callback notified when the popup state of the fusebox changes. */
    public NonNullObservableSupplier<@PopupState Integer> getPopupStateSupplier() {
        return mPopupStateSupplier;
    }

    /** Returns whether there are any attachments present in the current session. */
    public NonNullObservableSupplier<Boolean> getHasAttachmentsSupplier() {
        return mHasAttachmentsSupplier;
    }

    /** Set callback to be invoked when the popup is dismissed. */
    public void setOnInteractionCompletedCallback(Callback<Boolean> callback) {
        mOnInteractionCompletedCallback = callback;
    }

    /** Set callback to be invoked when the first picker interaction is canceled. */
    public void setOnFirstPickerInteractionCanceledCallback(Runnable callback) {
        mOnFirstPickerInteractionCanceledCallback = callback;
        if (mMediator != null) {
            mMediator.setOnFirstPickerInteractionCanceledCallback(callback);
        }
    }

    /**
     * Resolves the layout mode for the Fusebox. Forced phone-style Omniboxes (e.g. for hub, tab
     * search, or the search widget) use the TOOLBAR layout mode to match mobile layouts.
     */
    private @FuseboxLayoutMode int getFuseboxLayoutMode() {
        return !mIsForcedPhoneStyleOmnibox
                        && OmniboxCapabilities.isDesktopPlatform()
                        && OmniboxFeatures.sAndroidDesktopAimGate.isEnabled()
                ? FuseboxLayoutMode.SUGGESTIONS_POPOVER
                : FuseboxLayoutMode.TOOLBAR;
    }

    /**
     * Provider of the viewport for the fusebox popup window. This implementation treats the entire
     * window as available, ignoring e.g. ime insets which can reduce the available height to a very
     * small quantity using PopupWindow's default viewport rect.
     */
    static class ViewportRectProvider extends RectProvider
            implements ComponentCallbacks,
                    View.OnLayoutChangeListener,
                    InsetObserver.WindowInsetObserver {
        private final Activity mActivity;
        private final @Nullable InsetObserver mInsetObserver;
        private final View mView;

        public ViewportRectProvider(
                Activity activity, @Nullable InsetObserver insetObserver, View view) {
            mActivity = activity;
            mInsetObserver = insetObserver;
            if (mInsetObserver != null) {
                mInsetObserver.addObserver(this);
            }
            mView = view;
            mActivity.registerComponentCallbacks(this);
            mView.addOnLayoutChangeListener(this);
            updateRect();
        }

        @Override
        public void onInsetChanged() {
            updateRect();
        }

        @Override
        public void onConfigurationChanged(Configuration configuration) {
            updateRect();
        }

        @Override
        public void onLayoutChange(
                View v,
                int left,
                int top,
                int right,
                int bottom,
                int oldLeft,
                int oldTop,
                int oldRight,
                int oldBottom) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::updateRect);
        }

        private int getTopInset() {
            if (mInsetObserver == null) return 0;

            @Nullable WindowInsetsCompat insets = mInsetObserver.getLastRawWindowInsets();
            if (insets == null) return 0;

            int topInset = insets.getInsets(Type.systemBars()).top;
            return topInset;
        }

        private void updateRect() {
            var windowMetrics =
                    WindowMetricsCalculator.getOrCreate().computeCurrentWindowMetrics(mActivity);
            var bounds = windowMetrics.getBounds();
            Rect newRect = new Rect(0, getTopInset(), bounds.width(), bounds.height());
            if (!newRect.equals(mRect)) {
                mRect.set(newRect);
                notifyRectChanged();
            }
        }

        @Override
        public void onLowMemory() {}

        public void destroy() {
            mActivity.unregisterComponentCallbacks(this);
            mView.removeOnLayoutChangeListener(this);
            if (mInsetObserver != null) {
                mInsetObserver.removeObserver(this);
            }
        }
    }
}
