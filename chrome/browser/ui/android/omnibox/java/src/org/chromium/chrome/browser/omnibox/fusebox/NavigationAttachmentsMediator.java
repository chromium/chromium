// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.Manifest;
import android.app.Activity;
import android.content.ClipData;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.Bitmap;
import android.graphics.Bitmap.CompressFormat;
import android.graphics.BitmapFactory;
import android.graphics.drawable.BitmapDrawable;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Mediator for the Navigation Attachments component. */
@NullMarked
public class NavigationAttachmentsMediator {
    // TODO(crbug.com/457825183): Supply this class name and extra string externally.
    private static final String CHROME_ITEM_PICKER_ACTIVITY_CLASS =
            "org.chromium.chrome.browser.chrome_item_picker.ChromeItemPickerActivity";
    public static final String EXTRA_ATTACHMENT_TAB_IDS = "TAB_IDS";
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final PropertyModel mModel;
    private final FuseboxPopup mPopup;
    private final FuseboxAttachmentModelList mModelList;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private final ObservableSupplierImpl<Boolean> mOnCompactModeChangedSupplier;
    private final Callback<@AutocompleteRequestType Integer> mOnAutocompleteRequestTypeChanged =
            this::onAutocompleteRequestTypeChanged;
    private boolean mUseCompactUi;

    NavigationAttachmentsMediator(
            Context context,
            WindowAndroid windowAndroid,
            PropertyModel model,
            FuseboxViewHolder viewHolder,
            FuseboxAttachmentModelList modelList,
            ObservableSupplierImpl<@AutocompleteRequestType Integer>
                    autocompleteRequestTypeSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ComposeBoxQueryControllerBridge composeBoxQueryControllerBridge,
            ObservableSupplierImpl<Boolean> onCompactModeChangedSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mPermissionDelegate = windowAndroid;
        mModel = model;
        mPopup = viewHolder.popup;
        mModelList = modelList;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mAutocompleteRequestTypeSupplier = autocompleteRequestTypeSupplier;
        mComposeBoxQueryControllerBridge = composeBoxQueryControllerBridge;
        mOnCompactModeChangedSupplier = onCompactModeChangedSupplier;

        mAutocompleteRequestTypeSupplier.addObserver(mOnAutocompleteRequestTypeChanged);

        mModel.set(FuseboxProperties.BUTTON_ADD_CLICKED, this::onToggleAttachmentsPopup);
        mModel.set(FuseboxProperties.POPUP_CAMERA_CLICKED, this::onCameraClicked);
        mModel.set(FuseboxProperties.POPUP_GALLERY_CLICKED, this::onImagePickerClicked);
        mModel.set(FuseboxProperties.POPUP_FILE_CLICKED, this::onFilePickerClicked);
        mModel.set(FuseboxProperties.POPUP_CLIPBOARD_CLICKED, this::onClipboardClicked);
        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED,
                this::onRequestTypeButtonClicked);
        mModel.set(
                FuseboxProperties.POPUP_AI_MODE_CLICKED,
                () -> activateAiMode(AiModeActivationSource.TOOL_MENU));
        mModel.set(FuseboxProperties.POPUP_CREATE_IMAGE_CLICKED, this::activateImageGeneration);
        mModel.set(FuseboxProperties.POPUP_TAB_PICKER_CLICKED, this::onTabPickerClicked);

        mModel.set(
                FuseboxProperties.POPUP_FILE_BUTTON_VISIBLE,
                mComposeBoxQueryControllerBridge.isPdfUploadEligible());
        mModel.set(
                FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_VISIBLE,
                mComposeBoxQueryControllerBridge.isCreateImagesEligible());

        mModelList.addObserver(
                new ListObservable.ListObserver<>() {
                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        onAttachmentsChanged();
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        onAttachmentsChanged();
                    }
                });
        onAttachmentsChanged();
    }

    public void destroy() {
        mAutocompleteRequestTypeSupplier.removeObserver(mOnAutocompleteRequestTypeChanged);
    }

    private void onRequestTypeButtonClicked() {
        switch (mAutocompleteRequestTypeSupplier.get()) {
            case AutocompleteRequestType.AI_MODE:
            case AutocompleteRequestType.IMAGE_GENERATION:
                activateSearchMode();
                break;

            default:
                activateAiMode(AiModeActivationSource.DEDICATED_BUTTON);
                break;
        }
    }

    /** Activate Search as the Next Request fulfillment type. */
    void activateSearchMode() {
        mPopup.dismiss();
        if (mAutocompleteRequestTypeSupplier.get() == AutocompleteRequestType.SEARCH) return;
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);

        mModelList.clear();
    }

    /** Activate AI Mode as the Next Request fulfillment type. */
    void activateAiMode(@AiModeActivationSource int activationReason) {
        mPopup.dismiss();
        if (mAutocompleteRequestTypeSupplier.get() == AutocompleteRequestType.AI_MODE) return;
        FuseboxMetrics.notifyAiModeActivated(activationReason);
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);
    }

    /** Activate image generation as the Next Request fulfillment type. */
    void activateImageGeneration() {
        mPopup.dismiss();
        if (mAutocompleteRequestTypeSupplier.get() == AutocompleteRequestType.IMAGE_GENERATION) {
            return;
        }
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.IMAGE_GENERATION);
    }

    /**
     * Show or hide the navigation attachments toolbar.
     *
     * @param visible Whether the toolbar should be visible.
     */
    void setToolbarVisible(boolean visible) {
        mModel.set(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE, visible);
        setUseCompactUi(OmniboxFeatures.sCompactFusebox.getValue());
    }

    public void setAutocompleteRequestTypeChangeable(boolean isChangeable) {
        // Don't take an action if the state isn't really changing.
        if (mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE) == isChangeable)
            return;

        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, isChangeable);
        if (!isChangeable) {
            activateSearchMode();
        }
    }

    /**
     * @param url The search URL to get the AIM analog of.
     * @return The URL for the AIM service.
     */
    GURL getAimUrl(GURL url) {
        return mComposeBoxQueryControllerBridge.getAimUrl(url);
    }

    /**
     * @param url The search URL to get the Image generator analog of.
     * @param queryText The query text to be used for the image generation URL.
     * @return The URL for the image generation service.
     */
    GURL getImageGenerationUrl(GURL url) {
        return mComposeBoxQueryControllerBridge.getImageGenerationUrl(url);
    }

    @VisibleForTesting
    void onToggleAttachmentsPopup() {
        if (mPopup.isShowing()) {
            mPopup.dismiss();
        } else {
            updateModelForCurrentTab();
            mModel.set(
                    FuseboxProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE,
                    Clipboard.getInstance().hasImage());
            mPopup.show();
        }
        FuseboxMetrics.notifyAttachmentsPopupToggled(!mPopup.isShowing(), mModel);
    }

    private void updateModelForCurrentTab() {
        if (mTabModelSelectorSupplier.get() == null
                || mTabModelSelectorSupplier.get().getCurrentTab() == null) {
            mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, false);
            return;
        }

        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        assumeNonNull(tabModelSelector);
        Tab currentTab = assumeNonNull(tabModelSelector.getCurrentTab());
        boolean tabIsEligible =
                currentTab != null
                        && !currentTab.isIncognitoBranded()
                        && currentTab.isInitialized()
                        && !currentTab.isFrozen()
                        && (currentTab.getUrl().getScheme().equals(UrlConstants.HTTP_SCHEME)
                                || currentTab
                                        .getUrl()
                                        .getScheme()
                                        .equals(UrlConstants.HTTPS_SCHEME));

        if (tabIsEligible) {
            mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, true);
            mModel.set(
                    FuseboxProperties.CURRENT_TAB_BUTTON_CLICKED,
                    () -> onAddCurrentTab(currentTab));
            mModel.set(
                    FuseboxProperties.CURRENT_TAB_BUTTON_FAVICON,
                    OmniboxResourceProvider.getFaviconBitmapForTab(currentTab));
        } else {
            mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, false);
        }
    }

    private void onAddCurrentTab(Tab tab) {
        if (mComposeBoxQueryControllerBridge == null) return;
        activateAiMode(AiModeActivationSource.IMPLICIT);

        var attachment = FuseboxAttachment.forTab(tab, mContext.getResources());

        // Use FuseboxModelList's add method which handles upload automatically
        mModelList.add(attachment);
    }

    private void onAttachmentsChanged() {
        mModel.set(FuseboxProperties.ATTACHMENTS_VISIBLE, !mModelList.isEmpty());
        mModel.set(
                FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED,
                !attachmentsContainType(FuseboxAttachmentType.ATTACHMENT_TAB));
    }

    private boolean attachmentsContainType(@FuseboxAttachmentType int target) {
        for (MVCListAdapter.ListItem listItem : mModelList) {
            if (listItem.type == target) {
                return true;
            }
        }
        return false;
    }

    @VisibleForTesting
    void onTabPickerClicked() {
        mPopup.dismiss();
        Intent intent;
        try {
            intent = new Intent(mContext, Class.forName(CHROME_ITEM_PICKER_ACTIVITY_CLASS));
            if (mTabModelSelectorSupplier.get() != null
                    && mTabModelSelectorSupplier.get().getCurrentTab() != null) {
                ProfileIntentUtils.addProfileToIntent(
                        mTabModelSelectorSupplier.get().getCurrentTab().getProfile(), intent);
            }
        } catch (ClassNotFoundException e) {
            return;
        }

        mWindowAndroid.showCancelableIntent(
                intent, this::onTabPickerResult, R.string.low_memory_error);
    }

    void onTabPickerResult(int resultCode, @Nullable Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null || data.getExtras() == null) return;
        // Retrieve list of tab ids.
        long[] tabIds = data.getLongArrayExtra(EXTRA_ATTACHMENT_TAB_IDS);
        if (tabIds == null) return;
        for (long tabId : tabIds) {
            TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
            if (tabModelSelector == null) return;
            Tab tab = mTabModelSelectorSupplier.get().getTabById((int) tabId);
            if (tab == null) return;
            onAddCurrentTab(tab);
        }
    }

    @VisibleForTesting
    void onCameraClicked() {
        mPopup.dismiss();
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CAMERA);
        if (mPermissionDelegate.hasPermission(Manifest.permission.CAMERA)) {
            launchCamera();
        } else {
            mPermissionDelegate.requestPermissions(
                    new String[] {Manifest.permission.CAMERA},
                    (permissions, grantResults) -> {
                        if (grantResults.length > 0
                                && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                            launchCamera();
                        }
                    });
        }
    }

    private void onAutocompleteRequestTypeChanged(@AutocompleteRequestType Integer type) {
        setUseCompactUi(
                type == AutocompleteRequestType.SEARCH
                        && OmniboxFeatures.sCompactFusebox.getValue());
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, type);
        boolean tabInputsEnabled = type != AutocompleteRequestType.IMAGE_GENERATION;
        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED, tabInputsEnabled);
        // TODO(https://www.crbug.com/456274957): Also set enabled on select tabs
        // button.
    }

    @VisibleForTesting
    void launchCamera() {
        // Ask for a small-sized bitmap as a direct reply (passing no `EXTRA_OUTPUT` uri).
        // This should be sufficiently good, offering image of around 200-300px on the long edge.
        var i = new Intent(MediaStore.ACTION_IMAGE_CAPTURE);

        mWindowAndroid.showCancelableIntent(
                i,
                (resultCode, data) -> {
                    if (resultCode != Activity.RESULT_OK
                            || data == null
                            || data.getExtras() == null) {
                        return;
                    }

                    var bitmap = (Bitmap) data.getExtras().get("data");
                    if (bitmap == null) return;

                    ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
                    bitmap.compress(CompressFormat.PNG, 100, byteArrayOutputStream);
                    byte[] dataBytes = byteArrayOutputStream.toByteArray();
                    var attachment =
                            FuseboxAttachment.forCameraImage(
                                    new BitmapDrawable(mContext.getResources(), bitmap),
                                    "",
                                    "image/png",
                                    dataBytes);
                    uploadAndAddAttachment(attachment);
                },
                R.string.low_memory_error);
    }

    @VisibleForTesting
    void onImagePickerClicked() {
        mPopup.dismiss();

        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.GALLERY);
        Intent i;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            i =
                    new Intent(MediaStore.ACTION_PICK_IMAGES)
                            .setType(MimeTypeUtils.IMAGE_ANY_MIME_TYPE)
                            .putExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, 10);
        } else {
            i =
                    new Intent(Intent.ACTION_PICK)
                            .setDataAndType(
                                    MediaStore.Images.Media.INTERNAL_CONTENT_URI,
                                    MimeTypeUtils.IMAGE_ANY_MIME_TYPE)
                            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }

        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        mWindowAndroid.showCancelableIntent(
                i,
                (resultCode, data) -> {
                    if (resultCode != Activity.RESULT_OK || data == null) return;

                    var uris = extractUrisFromResult(data);
                    for (var uri : uris) {
                        fetchAttachmentDetails(
                                uri,
                                FuseboxAttachmentType.ATTACHMENT_IMAGE,
                                this::uploadAndAddAttachment);
                    }
                },
                R.string.low_memory_error);
    }

    @VisibleForTesting
    void onFilePickerClicked() {
        mPopup.dismiss();
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.FILES);
        var i =
                new Intent(Intent.ACTION_OPEN_DOCUMENT)
                        .addCategory(Intent.CATEGORY_OPENABLE)
                        .setType(MimeTypeUtils.PDF_MIME_TYPE)
                        .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                        .addFlags(
                                Intent.FLAG_GRANT_READ_URI_PERMISSION
                                        | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);

        mWindowAndroid.showCancelableIntent(
                i,
                (resultCode, data) -> {
                    if (resultCode != Activity.RESULT_OK || data == null) return;

                    var uris = extractUrisFromResult(data);
                    for (var uri : uris) {
                        fetchAttachmentDetails(
                                uri,
                                FuseboxAttachmentType.ATTACHMENT_FILE,
                                this::uploadAndAddAttachment);
                    }
                },
                /* errorId= */ android.R.string.cancel);
    }

    @VisibleForTesting
    void onClipboardClicked() {
        mPopup.dismiss();
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CLIPBOARD);
        new AsyncTask<byte[]>() {
            @Override
            protected byte[] doInBackground() {
                byte[] png = Clipboard.getInstance().getPng();
                return png == null ? new byte[0] : png;
            }

            @Override
            protected void onPostExecute(byte[] pngBytes) {
                if (pngBytes == null || pngBytes.length == 0) return;

                Bitmap bitmap = BitmapFactory.decodeByteArray(pngBytes, 0, pngBytes.length);
                if (bitmap == null) return;

                var attachment =
                        FuseboxAttachment.forCameraImage(
                                new BitmapDrawable(mContext.getResources(), bitmap),
                                "",
                                "image/png",
                                pngBytes);
                uploadAndAddAttachment(attachment);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    void fetchAttachmentDetails(
            Uri uri, @FuseboxAttachmentType int type, Callback<FuseboxAttachment> callback) {
        new FuseboxAttachmentDetailsFetcher(
                        mContext, mContext.getContentResolver(), uri, type, callback)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Add an attachment to the navigation attachments toolbar.
     *
     * @param attachmentDetails The details of the attachment to add.
     */
    /* package */ void uploadAndAddAttachment(FuseboxAttachment attachment) {
        activateAiMode(AiModeActivationSource.IMPLICIT);

        // Use FuseboxModelList's unified add method
        mModelList.add(attachment);
    }

    // Parse GET_CONTENT response, extracting single- or multiple image selections.
    private static List<Uri> extractUrisFromResult(Intent data) {
        List<Uri> out = new ArrayList<>();
        Uri single = data.getData();
        if (single != null) out.add(single);

        ClipData clip = data.getClipData();
        if (clip == null) return out;

        for (int i = 0; i < clip.getItemCount(); i++) {
            Uri u = clip.getItemAt(i).getUri();
            if (u != null) out.add(u);
        }
        return out;
    }

    /**
     * @return List of attachment tokens, empty if no attachments.
     */
    public List<String> getAttachmentTokens() {
        if (mModelList.size() == 0) return Collections.emptyList();
        List<String> tokens = new ArrayList<>();
        for (int i = 0; i < mModelList.size(); i++) {
            PropertyModel model = mModelList.get(i).model;
            var attachment = model.get(FuseboxAttachmentProperties.ATTACHMENT);
            tokens.add(attachment.getToken());
        }
        return tokens;
    }

    void setUseCompactUi(boolean useCompactUi) {
        if (mUseCompactUi == useCompactUi) return;
        mUseCompactUi = useCompactUi;
        mOnCompactModeChangedSupplier.set(mUseCompactUi);
        mModel.set(FuseboxProperties.COMPACT_UI, useCompactUi);
    }
}
