// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
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
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.url.GURL;

/** Coordinator for the Navigation Attachments component. */
@NullMarked
public class NavigationAttachmentsCoordinator implements UrlFocusChangeListener {
    private final @Nullable NavigationAttachmentsViewHolder mViewHolder;
    private final @Nullable LocationBarDataProvider mLocationBarDataProvider;
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier =
                    new ObservableSupplierImpl<>(AutocompleteRequestType.SEARCH);
    private final boolean mAimToggleOnly;
    private final PropertyModel mModel;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final ModelList mModelList = new ModelList();
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ModelList mTabAttachmentsModelList = new ModelList();
    private @Nullable NavigationAttachmentsMediator mMediator;
    private @Nullable ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;

    public NavigationAttachmentsCoordinator(
            Context context,
            WindowAndroid windowAndroid,
            ViewGroup parent,
            ObservableSupplier<Profile> profileObservableSupplier,
            LocationBarDataProvider locationBarDataProvider,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;

        if (!OmniboxFeatures.sOmniboxMultimodalInput.isEnabled()
                || parent.findViewById(R.id.location_bar_attachments_toolbar) == null) {
            mViewHolder = null;
            mLocationBarDataProvider = null;
            mAimToggleOnly = false;
            mModel = new PropertyModel();
            return;
        }

        mAimToggleOnly = OmniboxFeatures.sAimToggleOnly.getValue();
        mLocationBarDataProvider = locationBarDataProvider;

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
                                NavigationAttachmentsProperties
                                        .AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE,
                                false)
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
        if (mComposeBoxQueryControllerBridge != null) {
            mComposeBoxQueryControllerBridge.destroy();
            mComposeBoxQueryControllerBridge = null;
        }
    }

    /** Called when the URL focus changes. */
    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (mMediator == null || mLocationBarDataProvider == null) return;

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
        boolean shouldShowToolbar = isChangeable && !mAimToggleOnly;
        mMediator.setToolbarVisible(shouldShowToolbar);
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
}
