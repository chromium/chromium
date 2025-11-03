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
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.provider.MediaStore;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import com.google.common.collect.Iterables;
import com.google.common.collect.Ordering;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.AttachmentDetailsFetcher.AttachmentDetails;
import org.chromium.chrome.browser.omnibox.fusebox.NavigationAttachmentsRecyclerViewAdapter.NavigationAttachmentItemType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

/** Mediator for the Navigation Attachments component. */
@NullMarked
class NavigationAttachmentsMediator {
    private static final String MIMETYPE_IMAGE_ANY = "image/*";
    private static final int MAX_RECENT_TABS_TO_PRESENT = 5;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final PropertyModel mModel;
    private final NavigationAttachmentsPopup mPopup;
    private final ModelList mModelList;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ModelList mTabAttachmentsModelList;
    private final Drawable mFallbackDrawable;
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;

    NavigationAttachmentsMediator(
            Context context,
            WindowAndroid windowAndroid,
            PropertyModel model,
            NavigationAttachmentsViewHolder viewHolder,
            ModelList modelList,
            ObservableSupplierImpl<@AutocompleteRequestType Integer>
                    autocompleteRequestTypeSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ModelList tabAttachmentsModelList,
            ComposeBoxQueryControllerBridge composeBoxQueryControllerBridge) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mPermissionDelegate = windowAndroid;
        mModel = model;
        mPopup = viewHolder.popup;
        mModelList = modelList;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mTabAttachmentsModelList = tabAttachmentsModelList;
        mFallbackDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.ic_attach_file_24dp);
        mAutocompleteRequestTypeSupplier = autocompleteRequestTypeSupplier;
        mComposeBoxQueryControllerBridge = composeBoxQueryControllerBridge;

        mAutocompleteRequestTypeSupplier.addObserver(
                (type) ->
                        mModel.set(
                                NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE, type));

        mModel.set(
                NavigationAttachmentsProperties.BUTTON_ADD_CLICKED, this::onToggleAttachmentsPopup);
        mModel.set(NavigationAttachmentsProperties.POPUP_CAMERA_CLICKED, this::onCameraClicked);
        mModel.set(
                NavigationAttachmentsProperties.POPUP_GALLERY_CLICKED, this::onImagePickerClicked);
        mModel.set(NavigationAttachmentsProperties.POPUP_FILE_CLICKED, this::onFilePickerClicked);
        mModel.set(
                NavigationAttachmentsProperties.POPUP_CLIPBOARD_CLICKED, this::onClipboardClicked);
        mModel.set(
                NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED,
                this::onRequestTypeButtonClicked);
        mModel.set(NavigationAttachmentsProperties.POPUP_AI_MODE_CLICKED, this::activateAiMode);
    }

    private void onRequestTypeButtonClicked() {
        switch (mAutocompleteRequestTypeSupplier.get()) {
            case AutocompleteRequestType.AI_MODE:
                activateSearchMode();
                break;

            default:
                activateAiMode();
                break;
        }
    }

    /** Activate Search as the Next Request fulfillment type. */
    void activateSearchMode() {
        mPopup.dismiss();
        if (mAutocompleteRequestTypeSupplier.get() == AutocompleteRequestType.SEARCH) return;
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.SEARCH);

        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, false);
        mComposeBoxQueryControllerBridge.notifySessionAbandoned();
        mModelList.clear();
    }

    /** Activate AI Mode as the Next Request fulfillment type. */
    void activateAiMode() {
        mPopup.dismiss();
        if (mAutocompleteRequestTypeSupplier.get() == AutocompleteRequestType.AI_MODE) return;
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);

        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE, true);
        mComposeBoxQueryControllerBridge.notifySessionStarted();
    }

    /**
     * Show or hide the navigation attachments toolbar.
     *
     * @param visible Whether the toolbar should be visible.
     */
    void setToolbarVisible(boolean visible) {
        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE, visible);
    }

    public void setAutocompleteRequestTypeChangeable(boolean isChangeable) {
        // Don't take an action if the state isn't really changing.
        if (mModel.get(NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE)
                == isChangeable) return;

        mModel.set(
                NavigationAttachmentsProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE, isChangeable);
        if (!isChangeable) {
            activateSearchMode();
        }
    }

    /**
     * @return An {@link ObservableSupplier} that notifies observers when the autocomplete request
     *     type changes.
     */
    ObservableSupplier<@AutocompleteRequestType Integer> getAutocompleteRequestTypeSupplier() {
        return mAutocompleteRequestTypeSupplier;
    }

    /**
     * @param queryText The query text to be used for the AIM URL.
     * @return The URL for the AIM service.
     */
    GURL getAimUrl(String queryText) {
        return mComposeBoxQueryControllerBridge.getAimUrl(queryText);
    }

    @VisibleForTesting
    void onToggleAttachmentsPopup() {
        if (mPopup.isShowing()) {
            mPopup.dismiss();
        } else {
            buildModelListForRecentTabs();
            mModel.set(
                    NavigationAttachmentsProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE,
                    Clipboard.getInstance().hasImage());
            mPopup.show();
        }
    }

    private void buildModelListForRecentTabs() {
        mTabAttachmentsModelList.clear();
        if (mTabModelSelectorSupplier.get() == null) {
            mModel.set(NavigationAttachmentsProperties.RECENT_TABS_HEADER_VISIBLE, false);
            return;
        }

        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        assumeNonNull(tabModelSelector);
        Iterable<Tab> filteredTabs =
                Iterables.filter(
                        tabModelSelector.getCurrentModel(),
                        (tab) ->
                                !tab.isIncognitoBranded()
                                        && tab.isInitialized()
                                        && !tab.isFrozen()
                                        && (tab.getUrl()
                                                        .getScheme()
                                                        .equals(UrlConstants.HTTP_SCHEME)
                                                || tab.getUrl()
                                                        .getScheme()
                                                        .equals(UrlConstants.HTTPS_SCHEME)));
        List<Tab> tabs =
                Ordering.from(Comparator.comparingLong(Tab::getTimestampMillis))
                        .greatestOf(filteredTabs, MAX_RECENT_TABS_TO_PRESENT);
        for (Tab tab : tabs) {
            PropertyModel tabProperties =
                    new PropertyModel.Builder(TabAttachmentPopupChoiceProperties.ALL_KEYS)
                            .with(
                                    TabAttachmentPopupChoiceProperties.ON_CLICK_LISTENER,
                                    (v) -> onTabAttachmentClicked(tab))
                            .with(
                                    TabAttachmentPopupChoiceProperties.THUMBNAIL,
                                    new BitmapDrawable(
                                            mContext.getResources(),
                                            OmniboxResourceProvider.getFaviconBitmapForTab(tab)))
                            .with(TabAttachmentPopupChoiceProperties.TITLE, tab.getTitle())
                            .build();
            ListItem listItem =
                    new ListItem(
                            TabAttachmentPopupChoicesRecyclerViewAdapter.TAB_ATTACHMENT_ITEM_TYPE,
                            tabProperties);
            mTabAttachmentsModelList.add(listItem);
        }
        mModel.set(
                NavigationAttachmentsProperties.RECENT_TABS_HEADER_VISIBLE,
                !mTabAttachmentsModelList.isEmpty());
    }

    private void onTabAttachmentClicked(Tab tab) {
        if (mComposeBoxQueryControllerBridge == null) return;
        activateAiMode();
        @Nullable String token = mComposeBoxQueryControllerBridge.addTabContext(tab);
        if (TextUtils.isEmpty(token)) return;
        AttachmentDetails attachmentDetails =
                new AttachmentDetails(
                        NavigationAttachmentItemType.ATTACHMENT_TAB,
                        new BitmapDrawable(
                                mContext.getResources(),
                                OmniboxResourceProvider.getFaviconBitmapForTab(tab)),
                        tab.getTitle(),
                        /* mimeType= */ "",
                        /* data= */ new byte[] {});
        addAttachment(attachmentDetails, token);
    }

    @VisibleForTesting
    void onCameraClicked() {
        mPopup.dismiss();
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
                    AttachmentDetails attachmentDetails =
                            new AttachmentDetails(
                                    NavigationAttachmentItemType.ATTACHMENT_IMAGE,
                                    new BitmapDrawable(mContext.getResources(), bitmap),
                                    "",
                                    "image/png",
                                    dataBytes);
                    uploadAndAddAttachment(attachmentDetails);
                },
                R.string.low_memory_error);
    }

    @VisibleForTesting
    void onImagePickerClicked() {
        mPopup.dismiss();

        Intent i;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            i =
                    new Intent(MediaStore.ACTION_PICK_IMAGES)
                            .setType(MIMETYPE_IMAGE_ANY)
                            .putExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, 10);
        } else {
            i =
                    new Intent(Intent.ACTION_PICK)
                            .setDataAndType(
                                    MediaStore.Images.Media.INTERNAL_CONTENT_URI,
                                    MIMETYPE_IMAGE_ANY)
                            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true);
        }

        i.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        mWindowAndroid.showCancelableIntent(
                i,
                (resultCode, data) -> {
                    if (resultCode != Activity.RESULT_OK || data == null) return;

                    var uris = extractUrisFromResult(data);
                    for (var uri : uris) {
                        fetchAttachmentDetails(uri, this::uploadAndAddAttachment);
                    }
                },
                R.string.low_memory_error);
    }

    @VisibleForTesting
    void onFilePickerClicked() {
        mPopup.dismiss();
        var i =
                new Intent(Intent.ACTION_OPEN_DOCUMENT)
                        .addCategory(Intent.CATEGORY_OPENABLE)
                        .setType("*/*")
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
                        fetchAttachmentDetails(uri, this::uploadAndAddAttachment);
                    }
                },
                /* errorId= */ android.R.string.cancel);
    }

    @VisibleForTesting
    void onClipboardClicked() {
        mPopup.dismiss();
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

                AttachmentDetails attachmentDetails =
                        new AttachmentDetails(
                                NavigationAttachmentItemType.ATTACHMENT_IMAGE,
                                new BitmapDrawable(mContext.getResources(), bitmap),
                                "",
                                "image/png",
                                pngBytes);
                uploadAndAddAttachment(attachmentDetails);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    void fetchAttachmentDetails(
            Uri uri, Callback<AttachmentDetailsFetcher.AttachmentDetails> callback) {
        new AttachmentDetailsFetcher(mContext, mContext.getContentResolver(), uri, callback)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Add an attachment to the navigation attachments toolbar.
     *
     * @param attachmentDetails The details of the attachment to add.
     */
    /* package */ void uploadAndAddAttachment(
            AttachmentDetailsFetcher.AttachmentDetails attachmentDetails) {
        String token = uploadAttachment(attachmentDetails);
        if (TextUtils.isEmpty(token)) return;
        addAttachment(attachmentDetails, token);
    }

    private void addAttachment(
            AttachmentDetailsFetcher.AttachmentDetails attachmentDetails, String token) {
        activateAiMode();

        PropertyModel model =
                new PropertyModel.Builder(NavigationAttachmentItemProperties.ALL_KEYS)
                        .with(
                                NavigationAttachmentItemProperties.THUMBNAIL,
                                attachmentDetails.thumbnail != null
                                        ? attachmentDetails.thumbnail
                                        : mFallbackDrawable)
                        .with(NavigationAttachmentItemProperties.TITLE, attachmentDetails.title)
                        .with(
                                NavigationAttachmentItemProperties.DESCRIPTION,
                                attachmentDetails.mimeType)
                        .build();

        var listItem = new MVCListAdapter.ListItem(attachmentDetails.itemType, model);
        model.set(
                NavigationAttachmentItemProperties.ON_REMOVE,
                () -> removeAttachment(listItem, token));
        mModelList.add(listItem);
    }

    /**
     * Remove an attachment from the navigation attachments toolbar.
     *
     * @param token The token of the attachment to remove.
     */
    public void removeAttachment(ListItem item, String token) {
        mModelList.remove(item);
        mComposeBoxQueryControllerBridge.removeAttachment(token);
    }

    private @Nullable String uploadAttachment(AttachmentDetails attachmentDetails) {
        return mComposeBoxQueryControllerBridge.addFile(
                attachmentDetails.title, attachmentDetails.mimeType, attachmentDetails.data);
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
}
