// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.search_engines.TemplateUrlService.TemplateUrlServiceObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/** Coordinator for the Navigation Attachments component. */
@NullMarked
public class NavigationAttachmentsCoordinator
        implements UrlFocusChangeListener, TemplateUrlServiceObserver {
    private final @Nullable NavigationAttachmentsViewHolder mViewHolder;
    private final @Nullable LocationBarDataProvider mLocationBarDataProvider;
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier =
                    new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);
    private final PropertyModel mModel;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mModelList = new ModelList();
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ModelList mTabAttachmentsModelList = new ModelList();
    private @Nullable NavigationAttachmentsMediator mMediator;
    private @Nullable ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private boolean mDefaultSearchEngineIsGoogle = true;
    private TemplateUrlService mTemplateUrlService;

    public NavigationAttachmentsCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ViewGroup parent,
            ObservableSupplier<Profile> profileObservableSupplier,
            LocationBarDataProvider locationBarDataProvider,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            OneshotSupplier<TemplateUrlService> templateUrlServiceSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;

        if (!OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                || parent.findViewById(R.id.location_bar_attachments_toolbar) == null) {
            mViewHolder = null;
            mLocationBarDataProvider = null;
            mModel = new PropertyModel();
            return;
        }

        mLocationBarDataProvider = locationBarDataProvider;
        templateUrlServiceSupplier.onAvailable(this::onTemplateUrlServiceAvailable);

        var popup =
                new NavigationAttachmentsPopup(
                        mContext,
                        parent.findViewById(R.id.location_bar_attachments_add),
                        mTabAttachmentsModelList);
        mViewHolder = new NavigationAttachmentsViewHolder(parent, popup);

        var adapter = new NavigationAttachmentsRecyclerViewAdapter(mModelList);
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
        PropertyModelChangeProcessor.create(
                mModel, mViewHolder, NavigationAttachmentsViewBinder::bind);
        new OneShotCallback<>(profileObservableSupplier, this::onProfileAvailable);
    }

    @VisibleForTesting
    void onProfileAvailable(Profile profile) {
        // Reset previous Mediator instance in case we migrate to continuous Profile observing.
        mMediator = null;

        mComposeBoxQueryControllerBridge = ComposeBoxQueryControllerBridge.getForProfile(profile);
        if (mComposeBoxQueryControllerBridge == null) return;

        mMediator =
                new NavigationAttachmentsMediator(
                        mContext,
                        mWindowAndroid,
                        mModel,
                        assumeNonNull(mViewHolder),
                        mModelList,
                        mAutocompleteRequestTypeSupplier,
                        mTabModelSelectorSupplier,
                        mTabAttachmentsModelList,
                        mComposeBoxQueryControllerBridge);
    }

    public void destroy() {
        mMediator = null;
        if (mTemplateUrlService != null) {
            mTemplateUrlService.removeObserver(this);
        }
        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.destroy();
            mComposeBoxQueryControllerBridge = null;
        }
    }

    /** Called when the URL focus changes. */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (mMediator == null
                || mLocationBarDataProvider == null
                || !mDefaultSearchEngineIsGoogle) {
            return;
        }

        int pageClass =
                mLocationBarDataProvider.getPageClassification(AutocompleteRequestType.SEARCH);

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
    public GURL getAimUrl(String queryText) {
        if (mMediator == null) return GURL.emptyGURL();
        return mMediator.getAimUrl(queryText);
    }

    public PropertyModel getModelForTesting() {
        return mModel;
    }

    @Nullable NavigationAttachmentsViewHolder getViewHolderForTesting() {
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
     * Whether the given mode allows "conventioanl" fulfillment of a valid typed url, i.e.
     * navigating to that url directly. As an example of where this might return false: if if the
     * user types www.foo.com and presses enter with this mode active, they will be taken to some
     * DSE-specific landing page where www.foo.com is the input, not directly to foo.com *
     */
    public static boolean isConventionalFulfillmentType(@AutocompleteRequestType int mode) {
        return mode == AutocompleteRequestType.SEARCH;
    }
}
