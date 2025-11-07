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

import androidx.annotation.Px;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

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
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.List;

/** Mediator for the Navigation Attachments component. */
@NullMarked
class NavigationAttachmentsMediator {
    // TODO(crbug.com/457825183): Supply this class name externally.
    private static final String CHROME_ITEM_PICKER_ACTIVITY_CLASS =
            "org.chromium.chrome.browser.chrome_item_picker.ChromeItemPickerActivity";
    private static final String MIMETYPE_IMAGE_ANY = "image/*";
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final PropertyModel mModel;
    private final NavigationAttachmentsPopup mPopup;
    private final ModelList mModelList;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final Drawable mFallbackDrawable;
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private final @Px int mPopupItemIconSizePx;

    NavigationAttachmentsMediator(
            Context context,
            WindowAndroid windowAndroid,
            PropertyModel model,
            NavigationAttachmentsViewHolder viewHolder,
            ModelList modelList,
            ObservableSupplierImpl<@AutocompleteRequestType Integer>
                    autocompleteRequestTypeSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ComposeBoxQueryControllerBridge composeBoxQueryControllerBridge) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mPermissionDelegate = windowAndroid;
        mModel = model;
        mPopup = viewHolder.popup;
        mModelList = modelList;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mFallbackDrawable =
                AppCompatResources.getDrawable(mContext, R.drawable.ic_attach_file_24dp);
        mAutocompleteRequestTypeSupplier = autocompleteRequestTypeSupplier;
        mComposeBoxQueryControllerBridge = composeBoxQueryControllerBridge;
        mPopupItemIconSizePx =
                mContext.getResources().getDimensionPixelSize(R.dimen.fusebox_popup_item_icon_size);

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
        mModel.set(
                NavigationAttachmentsProperties.POPUP_TAB_PICKER_CLICKED, this::onTabPickerClicked);

        mModelList.addObserver(
                new ListObservable.ListObserver<>() {
                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        mModel.set(
                                NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE,
                                !mModelList.isEmpty());
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        mModel.set(
                                NavigationAttachmentsProperties.ATTACHMENTS_VISIBLE,
                                !mModelList.isEmpty());
                    }
                });
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

        mComposeBoxQueryControllerBridge.notifySessionAbandoned();
        mModelList.clear();
    }

    /** Activate AI Mode as the Next Request fulfillment type. */
    void activateAiMode() {
        mPopup.dismiss();
        if (mAutocompleteRequestTypeSupplier.get() == AutocompleteRequestType.AI_MODE) return;
        mAutocompleteRequestTypeSupplier.set(AutocompleteRequestType.AI_MODE);

        mComposeBoxQueryControllerBridge.notifySessionStarted();
    }

    /**
     * Show or hide the navigation attachments toolbar.
     *
     * @param visible Whether the toolbar should be visible.
     */
    void setToolbarVisible(boolean visible) {
        mModel.set(NavigationAttachmentsProperties.ATTACHMENTS_TOOLBAR_VISIBLE, visible);
        mModel.set(
                NavigationAttachmentsProperties.COMPACT_UI,
                OmniboxFeatures.sCompactFusebox.getValue());
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
            updateModelForCurrentTab();
            mModel.set(
                    NavigationAttachmentsProperties.POPUP_CLIPBOARD_BUTTON_VISIBLE,
                    Clipboard.getInstance().hasImage());
            mPopup.show();
        }
    }

    private void updateModelForCurrentTab() {
        if (mTabModelSelectorSupplier.get() == null
                || mTabModelSelectorSupplier.get().getCurrentTab() == null) {
            mModel.set(NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_VISIBLE, false);
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
            mModel.set(NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_VISIBLE, true);
            mModel.set(
                    NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_CLICKED,
                    () -> onAddCurrentTab(currentTab));
            Drawable drawable;
            var favicon = OmniboxResourceProvider.getFaviconBitmapForTab(currentTab);
            if (favicon != null) {
                var bitmap =
                        Bitmap.createScaledBitmap(
                                favicon,
                                mPopupItemIconSizePx,
                                mPopupItemIconSizePx,
                                /* filter= */ true);
                drawable = new BitmapDrawable(mContext.getResources(), bitmap);
                drawable.setBounds(0, 0, mPopupItemIconSizePx, mPopupItemIconSizePx);
                mModel.set(NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_TINT, null);
            } else {
                drawable = assumeNonNull(mContext.getDrawable(R.drawable.ic_globe_24dp));
                mModel.set(
                        NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_TINT,
                        mContext.getColorStateList(R.color.default_icon_color_tint_list));
            }

            mModel.set(NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_THUMBNAIL, drawable);
        } else {
            mModel.set(NavigationAttachmentsProperties.CURRENT_TAB_BUTTON_VISIBLE, false);
        }
    }

    private void onAddCurrentTab(Tab tab) {
        if (mComposeBoxQueryControllerBridge == null) return;
        activateAiMode();
        @Nullable String token;
        // Web contents can be null when a tab has not been reloaded during the current Chrome
        // session. In this case, try to fetch cached web contents.
        if (tab.getWebContents() != null) {
            token = mComposeBoxQueryControllerBridge.addTabContext(tab);
        } else {
            token = mComposeBoxQueryControllerBridge.addTabContextFromCache(tab.getId());
        }

        addTabAttachment(tab, token);
    }

    private void addTabAttachment(Tab tab, @Nullable String token) {
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
    void onTabPickerClicked() {
        mPopup.dismiss();
        Intent intent;
        try {
            intent = new Intent(mContext, Class.forName(CHROME_ITEM_PICKER_ACTIVITY_CLASS));
        } catch (ClassNotFoundException e) {
            return;
        }

        mWindowAndroid.showCancelableIntent(
                intent,
                (resultCode, data) -> onTabPickerResult(resultCode, data),
                R.string.low_memory_error);
    }

    void onTabPickerResult(int resultCode, @Nullable Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null || data.getExtras() == null) return;
        // Retrieve list of tab ids.
        // TODO(haileywang): Fill with the real intent extra string when available.
        long[] tabIds = data.getLongArrayExtra("TAB_IDS");
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
                        fetchAttachmentDetails(
                                uri,
                                NavigationAttachmentItemType.ATTACHMENT_IMAGE,
                                this::uploadAndAddAttachment);
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
                        fetchAttachmentDetails(
                                uri,
                                NavigationAttachmentItemType.ATTACHMENT_ITEM,
                                this::uploadAndAddAttachment);
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
            Uri uri,
            @NavigationAttachmentItemType int type,
            Callback<AttachmentDetailsFetcher.AttachmentDetails> callback) {
        new AttachmentDetailsFetcher(mContext, mContext.getContentResolver(), uri, type, callback)
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
