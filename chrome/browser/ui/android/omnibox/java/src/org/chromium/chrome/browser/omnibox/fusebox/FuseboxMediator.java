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
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileIntentUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.search_engines.TemplateUrlService;
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
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/** Mediator for the Fusebox component. */
@NullMarked
public class FuseboxMediator {
    // TODO(crbug.com/457825183): Supply this class name and extra strings externally.
    @VisibleForTesting
    /* package */ static final String CHROME_ITEM_PICKER_ACTIVITY_CLASS =
            "org.chromium.chrome.browser.chrome_item_picker.ChromeItemPickerActivity";

    public static final String EXTRA_PRESELECTED_TAB_IDS = "EXTRA_PRESELECTED_TAB_IDS";
    public static final String EXTRA_IS_INCOGNITO_BRANDED = "EXTRA_IS_INCOGNITO_BRANDED";
    public static final String EXTRA_ATTACHMENT_TAB_IDS = "TAB_IDS";
    public static final String EXTRA_ALLOWED_SELECTION_COUNT = "ALLOWED_SELECTION_COUNT";

    private final Context mContext;
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final PropertyModel mModel;
    private final FuseboxPopup mPopup;
    private final FuseboxAttachmentModelList mModelList;
    private final ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ObservableSupplierImpl<@AutocompleteRequestType Integer>
            mAutocompleteRequestTypeSupplier;
    private final ComposeBoxQueryControllerBridge mComposeBoxQueryControllerBridge;
    private final ObservableSupplierImpl<@FuseboxState Integer> mFuseboxStateSupplier;
    private final Callback<@AutocompleteRequestType Integer> mOnAutocompleteRequestTypeChanged =
            this::onAutocompleteRequestTypeChanged;
    private final SnackbarManager mSnackbarManager;
    private final Supplier<@Nullable TemplateUrlService> mTemplateUrlServiceSupplier;
    private final Snackbar mAttachmentLimitSnackbar;
    private final Snackbar mAttachmentUploadFailedSnackbar;
    private final ObservableSupplierImpl<Boolean> mAttachmentsPresentSupplier;

    FuseboxMediator(
            Context context,
            Profile profile,
            WindowAndroid windowAndroid,
            PropertyModel model,
            FuseboxViewHolder viewHolder,
            FuseboxAttachmentModelList modelList,
            ObservableSupplierImpl<@AutocompleteRequestType Integer>
                    autocompleteRequestTypeSupplier,
            ObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ComposeBoxQueryControllerBridge composeBoxQueryControllerBridge,
            ObservableSupplierImpl<@FuseboxState Integer> fuseboxStateSupplier,
            ObservableSupplierImpl<Boolean> attachmentsPresentSupplier,
            SnackbarManager snackbarManager,
            Supplier<@Nullable TemplateUrlService> templateUrlServiceSupplier) {
        mContext = context;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mPermissionDelegate = windowAndroid;
        mModel = model;
        mPopup = viewHolder.popup;
        mModelList = modelList;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mAutocompleteRequestTypeSupplier = autocompleteRequestTypeSupplier;
        mComposeBoxQueryControllerBridge = composeBoxQueryControllerBridge;
        mFuseboxStateSupplier = fuseboxStateSupplier;
        mAttachmentsPresentSupplier = attachmentsPresentSupplier;
        mSnackbarManager = snackbarManager;
        mTemplateUrlServiceSupplier = templateUrlServiceSupplier;

        mAutocompleteRequestTypeSupplier.addObserver(mOnAutocompleteRequestTypeChanged);

        CharSequence snackbarLimitText = context.getText(R.string.fusebox_max_attachments);
        mAttachmentLimitSnackbar =
                Snackbar.make(
                        snackbarLimitText,
                        null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_FUSEBOX_MAX_ATTACHMENTS);
        CharSequence snackbarUploadFailedText = context.getText(R.string.fusebox_upload_failed);
        mAttachmentUploadFailedSnackbar =
                Snackbar.make(
                        snackbarUploadFailedText,
                        null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_FUSEBOX_UPLOAD_FAILED);

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

    /** Apply a variant of the branded color scheme to Fusebox UI elements */
    /*package */ void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        mModel.set(FuseboxProperties.COLOR_SCHEME, brandedColorScheme);
        mModelList.updateVisualsForState(brandedColorScheme);
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

    /** Activate AI Mode if no other custom mode is already active. */
    void maybeActivateAiMode(@AiModeActivationSource int activationReason) {
        mPopup.dismiss();
        if (mAutocompleteRequestTypeSupplier.get() != AutocompleteRequestType.SEARCH) return;
        activateAiMode(activationReason);
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
     * Show or hide the Fusebox toolbar.
     *
     * @param visible Whether the toolbar should be visible.
     */
    void setToolbarVisible(boolean visible) {
        mModel.set(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE, visible);
        // The omnibox can become focused with the autocomplete request type immediately set
        // to AI_MODE. We check the mode here to avoid erroneously staying in compact mode.
        setUseCompactUi(
                OmniboxFeatures.sCompactFusebox.getValue()
                        && mAutocompleteRequestTypeSupplier.get()
                                == AutocompleteRequestType.SEARCH);
    }

    public void setAutocompleteRequestTypeChangeable(boolean isChangeable) {
        // Don't take an action if the state isn't really changing.
        if (mModel.get(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CHANGEABLE) == isChangeable) {
            return;
        }

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
        var tabSelector = mTabModelSelectorSupplier.get();
        var shouldShowCurrentTab =
                tabSelector != null
                        && tabSelector.getCurrentTab() != null
                        && !mModelList
                                .getAttachedTabIds()
                                .contains(tabSelector.getCurrentTab().getId());

        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_VISIBLE, shouldShowCurrentTab);
        if (!shouldShowCurrentTab) return;

        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        assumeNonNull(tabModelSelector);
        Tab currentTab = assumeNonNull(tabModelSelector.getCurrentTab());
        TemplateUrlService templateUrlService = mTemplateUrlServiceSupplier.get();
        boolean tabIsEligible =
                FuseboxTabUtils.isTabEligibleForAttachment(currentTab, templateUrlService)
                        && FuseboxTabUtils.isTabActive(currentTab)
                        && !currentTab.isIncognitoBranded();
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
        maybeActivateAiMode(AiModeActivationSource.IMPLICIT);

        Set<Integer> currentAttachedIds = mModelList.getAttachedTabIds();
        if (currentAttachedIds.contains(tab.getId())) return;
        var attachment = FuseboxAttachment.forTab(tab, mContext.getResources());

        // Use FuseboxModelList's add method which handles upload automatically
        if (!mModelList.add(attachment)) {
            warnForMaxAttachments();
        }
    }

    private void onAttachmentsChanged() {
        mAttachmentsPresentSupplier.set(!mModelList.isEmpty());
        mModel.set(FuseboxProperties.ATTACHMENTS_VISIBLE, !mModelList.isEmpty());
        mModel.set(
                FuseboxProperties.POPUP_CREATE_IMAGE_BUTTON_ENABLED,
                areAttachmentsCompatibleWithCreateImage());
    }

    private boolean areAttachmentsCompatibleWithCreateImage() {
        int imageCount = 0;
        for (MVCListAdapter.ListItem listItem : mModelList) {
            if (listItem.type == FuseboxAttachmentType.ATTACHMENT_FILE) {
                return false;
            }
            if (listItem.type == FuseboxAttachmentType.ATTACHMENT_TAB) {
                return false;
            }
            if (listItem.type == FuseboxAttachmentType.ATTACHMENT_IMAGE) {
                imageCount++;
            }
        }
        return imageCount <= 1;
    }

    @VisibleForTesting
    void onTabPickerClicked() {
        mPopup.dismiss();
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.TAB_PICKER);
        int remainingAttachments = mModelList.getRemainingAttachments();
        if (remainingAttachments < 1) {
            warnForMaxAttachments();
            return;
        }
        Intent intent;
        ArrayList<Integer> preselectedTabIds = new ArrayList<>(mModelList.getAttachedTabIds());
        try {
            intent =
                    new Intent(mContext, Class.forName(CHROME_ITEM_PICKER_ACTIVITY_CLASS))
                            .putIntegerArrayListExtra(EXTRA_PRESELECTED_TAB_IDS, preselectedTabIds);
            ProfileIntentUtils.addProfileToIntent(mProfile, intent);

            TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
            boolean isIncognitoBrandedModelSelected = false;
            if (tabModelSelector != null) {
                isIncognitoBrandedModelSelected =
                        tabModelSelector.isIncognitoBrandedModelSelected();
            }
            intent.putExtra(EXTRA_IS_INCOGNITO_BRANDED, isIncognitoBrandedModelSelected);
        } catch (ClassNotFoundException e) {
            return;
        }

        int maxAllowedTabs = preselectedTabIds.size() + remainingAttachments;
        intent.putExtra(EXTRA_ALLOWED_SELECTION_COUNT, maxAllowedTabs);

        mWindowAndroid.showCancelableIntent(
                intent, this::onTabPickerResult, R.string.low_memory_error);
    }

    void onTabPickerResult(int resultCode, @Nullable Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null || data.getExtras() == null) return;
        ArrayList<Integer> tabIds = data.getIntegerArrayListExtra(EXTRA_ATTACHMENT_TAB_IDS);
        // tabIds will be null when the activity finishes with cancel using the back button.
        if (tabIds == null) return;
        updateCurrentlyAttachedTabs(new HashSet<>(tabIds));
        if (mModelList.size() != 0) {
            maybeActivateAiMode(AiModeActivationSource.IMPLICIT);
        }
    }

    void onAttachmentUploadFailed() {
        mSnackbarManager.showSnackbar(mAttachmentUploadFailedSnackbar);
    }

    /**
     * Reconciles the model list attachments with a new set of selected tab IDs by removing
     * deselected tabs and adding newly selected tabs in one pass.
     *
     * @param newlySelectedTabIds The set of Tab IDs (as Integer) that are now selected by the user.
     */
    @VisibleForTesting
    public void updateCurrentlyAttachedTabs(Set<Integer> newlySelectedTabIds) {
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        if (tabModelSelector == null) return;
        Set<Integer> currentAttachedIds = mModelList.getAttachedTabIds();
        mModelList.removeIf(
                item -> {
                    if (item.type != FuseboxAttachmentType.ATTACHMENT_TAB) return false;
                    FuseboxAttachment attachment =
                            item.model.get(FuseboxAttachmentProperties.ATTACHMENT);
                    Integer tabId = assumeNonNull(attachment).tabId;
                    return !newlySelectedTabIds.contains(tabId);
                });

        for (int id : newlySelectedTabIds) {
            if (!currentAttachedIds.contains(id)) {
                Tab tab = tabModelSelector.getTabById(id);
                boolean addFailed =
                        !mModelList.add(
                                FuseboxAttachment.forTab(
                                        assumeNonNull(tab), mContext.getResources()));
                if (addFailed) {
                    warnForMaxAttachments();
                    break;
                }
            }
        }
    }

    @VisibleForTesting
    void onCameraClicked() {
        mPopup.dismiss();
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CAMERA);
        if (mModelList.getRemainingAttachments() < 1) {
            warnForMaxAttachments();
            return;
        }
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
        boolean allowNonImage = type != AutocompleteRequestType.IMAGE_GENERATION;
        mModel.set(FuseboxProperties.CURRENT_TAB_BUTTON_ENABLED, allowNonImage);
        mModel.set(FuseboxProperties.POPUP_FILE_BUTTON_ENABLED, allowNonImage);
        mModel.set(FuseboxProperties.POPUP_TAB_PICKER_ENABLED, allowNonImage);
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
        if (mModelList.getRemainingAttachments() < 1) {
            warnForMaxAttachments();
            return;
        }

        boolean allowMultipleAttachments =
                mAutocompleteRequestTypeSupplier.get() != AutocompleteRequestType.IMAGE_GENERATION;
        Intent intent;
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            int imageMax = allowMultipleAttachments ? mModelList.getRemainingAttachments() : 1;
            intent =
                    new Intent(MediaStore.ACTION_PICK_IMAGES)
                            .setType(MimeTypeUtils.IMAGE_ANY_MIME_TYPE)
                            .putExtra(MediaStore.EXTRA_PICK_IMAGES_MAX, imageMax);
        } else {
            intent =
                    new Intent(Intent.ACTION_PICK)
                            .setDataAndType(
                                    MediaStore.Images.Media.INTERNAL_CONTENT_URI,
                                    MimeTypeUtils.IMAGE_ANY_MIME_TYPE)
                            .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, allowMultipleAttachments);
        }
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);

        mWindowAndroid.showCancelableIntent(
                intent,
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
        if (mModelList.getRemainingAttachments() < 1) {
            warnForMaxAttachments();
            return;
        }
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
        if (mModelList.getRemainingAttachments() < 1) {
            warnForMaxAttachments();
            return;
        }
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

    private void warnForMaxAttachments() {
        mSnackbarManager.showSnackbar(mAttachmentLimitSnackbar);
    }

    /**
     * Add an attachment to the Fusebox toolbar.
     *
     * @param attachment Contains information about the input that will be added as context.
     */
    /* package */ void uploadAndAddAttachment(FuseboxAttachment attachment) {
        // Image generation is only allowed to have a single piece of context.
        if (mAutocompleteRequestTypeSupplier.get() == AutocompleteRequestType.IMAGE_GENERATION) {
            mModelList.clear();
        }

        // Use FuseboxModelList's unified add method.
        if (!mModelList.add(attachment)) {
            warnForMaxAttachments();
        }
        maybeActivateAiMode(AiModeActivationSource.IMPLICIT);
    }

    // Parse GET_CONTENT response, extracting single- or multiple image selections.
    private static List<Uri> extractUrisFromResult(Intent data) {
        List<Uri> out = new ArrayList<>();
        ClipData clip = data.getClipData();
        if (clip != null) {
            for (int i = 0; i < clip.getItemCount(); i++) {
                Uri u = clip.getItemAt(i).getUri();
                if (u != null) out.add(u);
            }
        } else {
            Uri single = data.getData();
            if (single != null) out.add(single);
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
        boolean fuseboxActive = mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE);
        mFuseboxStateSupplier.set(
                fuseboxActive
                        ? useCompactUi ? FuseboxState.COMPACT : FuseboxState.EXPANDED
                        : FuseboxState.DISABLED);
        mModel.set(FuseboxProperties.COMPACT_UI, useCompactUi);
    }
}
