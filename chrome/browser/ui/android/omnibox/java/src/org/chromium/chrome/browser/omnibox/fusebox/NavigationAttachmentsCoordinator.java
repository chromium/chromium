// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.LayoutInflater;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.LocationBarDataProvider;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.url.GURL;

import java.util.Collections;
import java.util.List;

/** Coordinator for the Navigation Attachments component. */
@NullMarked
public class NavigationAttachmentsCoordinator
        implements UrlFocusChangeListener, TemplateUrlServiceObserver {
    private final @Nullable FuseboxViewHolder mViewHolder;
    private final @Nullable LocationBarDataProvider mLocationBarDataProvider;
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final PropertyModel mModel;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final FuseboxAttachmentModelList mModelList = new FuseboxAttachmentModelList();
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private @Nullable NavigationAttachmentsMediator mMediator;
    private @Nullable ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private boolean mDefaultSearchEngineIsGoogle = true;
    private TemplateUrlService mTemplateUrlService;
    private final ObservableSupplierImpl<Boolean> mOnCompactModeChangedSupplier =
            new ObservableSupplierImpl<>(false);

    public NavigationAttachmentsCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ConstraintLayout parent,
            ObservableSupplier<Profile> profileObservableSupplier,
            LocationBarDataProvider locationBarDataProvider,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier,
            ObservableSupplierImpl<@AutocompleteRequestType Integer>
                    autocompleteRequestTypeSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mAutocompleteRequestTypeSupplier = autocompleteRequestTypeSupplier;

        if (!OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                || parent.findViewById(R.id.fusebox_request_type) == null) {
            mViewHolder = null;
            mLocationBarDataProvider = null;
            mModel = new PropertyModel();
            return;
        }

        mLocationBarDataProvider = locationBarDataProvider;
        templateUrlServiceSupplier.onAvailable(this::onTemplateUrlServiceAvailable);

        var contextButton = parent.findViewById(R.id.location_bar_attachments_add);
        var rectProvider = new ViewRectProvider(contextButton);
        var popupView = LayoutInflater.from(context).inflate(R.layout.fusebox_context_popup, null);
        var popupWindow =
                new AnchoredPopupWindow(
                        mContext,
                        contextButton.getRootView(),
                        AppCompatResources.getDrawable(context, R.drawable.menu_bg_tinted),
                        popupView,
                        rectProvider);
        popupWindow.setOutsideTouchable(true);
        popupWindow.setAnimateFromAnchor(true);

        var popup = new FuseboxPopup(mContext, popupWindow, popupView);
        mViewHolder = new FuseboxViewHolder(parent, popup);

        var adapter = new FuseboxAttachmentRecyclerViewAdapter(mModelList);
        mViewHolder.attachmentsView.setAdapter(adapter);

        mModel =
                new PropertyModel.Builder(NavigationAttachmentsProperties.ALL_KEYS)
                        .with(NavigationAttachmentsProperties.ADAPTER, adapter)
                        .with(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE, false)
                        .with(
                                NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE,
                                AutocompleteRequestType.SEARCH)
                        .with(
                                NavigationAttachmentsProperties
                                        .AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE,
                                false)
                        .with(
                                NavigationAttachmentsProperties.SHOW_DEDICATED_MODE_BUTTON,
                                OmniboxFeatures.sShowDedicatedModeButton.getValue())
                        .build();
        PropertyModelChangeProcessor.create(mModel, mViewHolder, FuseboxViewBinder::bind);
        new OneShotCallback<>(profileObservableSupplier, this::onProfileAvailable);
    }

    @VisibleForTesting
    void onProfileAvailable(Profile profile) {
        // Reset previous Mediator instance in case we migrate to continuous Profile observing.
        mMediator = null;

        mComposeBoxQueryControllerBridge = ComposeBoxQueryControllerBridge.getForProfile(profile);
        AutocompleteController.getForProfile(profile)
                .setComposeboxQueryControllerBridge(mComposeBoxQueryControllerBridge);
        if (mComposeBoxQueryControllerBridge == null) return;

        // Set the bridge for the model list to enable tight coupling
        mModelList.setComposeBoxQueryControllerBridge(mComposeBoxQueryControllerBridge);

        mMediator =
                new NavigationAttachmentsMediator(
                        mContext,
                        mWindowAndroid,
                        mModel,
                        assumeNonNull(mViewHolder),
                        mModelList,
                        mAutocompleteRequestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mComposeBoxQueryControllerBridge,
                        mOnCompactModeChangedSupplier);
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
        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.destroy();
            mComposeBoxQueryControllerBridge = null;
        }
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

    void setMediatorForTesting(NavigationAttachmentsMediator mediator) {
        mMediator = mediator;
    }

    @Nullable NavigationAttachmentsMediator getMediatorForTesting() {
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
        mMediator.setUseCompactUi(!isWrapping);
    }

    /**
     * @return List of attachment tokens, empty if no attachments or mediator unavailable.
     */
    public List<String> getAttachmentTokens() {
        if (mMediator == null) {
            return Collections.emptyList();
        }
        return mMediator.getAttachmentTokens();
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
    public ObservableSupplier<Boolean> getOnCompactModeChangedSupplier() {
        return mOnCompactModeChangedSupplier;
    }
}
