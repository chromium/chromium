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
import android.os.SystemClock;
import android.provider.MediaStore;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
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
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.util.ChromeItemPickerExtras;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.url.GURL;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Mediator for the Fusebox component. */
@NullMarked
public class FuseboxMediator {
    private final Context mContext;
    private final Profile mProfile;
    private final WindowAndroid mWindowAndroid;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final PropertyModel mModel;
    private final FuseboxPopup mPopup;
    private final FuseboxAttachmentModelList mModelList;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier;
    private final Callback<@AutocompleteRequestType Integer> mOnAutocompleteRequestTypeChanged =
            this::onAutocompleteRequestTypeChanged;
    private final SnackbarManager mSnackbarManager;
    private final Snackbar mAttachmentUploadFailedSnackbar;
    private final ListObserver<Void> mListObserver =
            new ListObserver<>() {
                @Override
                public void onItemRangeInserted(ListObservable source, int index, int count) {
                    onAttachmentsChanged();
                }

                @Override
                public void onItemRangeRemoved(ListObservable source, int index, int count) {
                    onAttachmentsChanged();
                }
            };
    private @Nullable AutocompleteInput mInput;

    FuseboxMediator(
            Context context,
            Profile profile,
            WindowAndroid windowAndroid,
            PropertyModel model,
            FuseboxViewHolder viewHolder,
            FuseboxAttachmentModelList modelList,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            ComposeboxQueryControllerBridge composeBoxQueryControllerBridge,
            SettableNonNullObservableSupplier<@FuseboxState Integer> fuseboxStateSupplier,
            SnackbarManager snackbarManager) {
        mContext = context;
        mProfile = profile;
        mWindowAndroid = windowAndroid;
        mPermissionDelegate = windowAndroid;
        mModel = model;
        mPopup = viewHolder.popup;
        mModelList = modelList;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mComposeboxQueryControllerBridge = composeBoxQueryControllerBridge;
        mFuseboxStateSupplier = fuseboxStateSupplier;
        mSnackbarManager = snackbarManager;

        // Create the upload failed snackbar.
        mAttachmentUploadFailedSnackbar =
                createStyledSnackbar(
                        context.getText(R.string.fusebox_upload_failed),
                        Snackbar.UMA_FUSEBOX_UPLOAD_FAILED);

        mModel.set(FuseboxProperties.BUTTON_ADD_CLICKED, this::onToggleAttachmentsPopup);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED, this::onCameraClicked);
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED, this::onImagePickerClicked);
        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_CLICKED, this::onFilePickerClicked);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_CLICKED, this::onClipboardClicked);
        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED,
                this::onRequestTypeButtonClicked);
        mModel.set(
                FuseboxProperties.POPUP_TOOL_AI_MODE_CLICKED,
                () -> activateAiMode(AiModeActivationSource.TOOL_MENU));
        mModel.set(
                FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_CLICKED, this::activateImageGeneration);
        mModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED, this::onTabPickerClicked);
        mModel.set(
                FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE,
                mComposeboxQueryControllerBridge.isPdfUploadEligible());
        mModel.set(
                FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_VISIBLE,
                mComposeboxQueryControllerBridge.isCreateImagesEligible()
                        && (OmniboxFeatures.sShowImageGenerationButtonInIncognito.getValue()
                                || !profile.isIncognitoBranded()));

        mModelList.addObserver(mListObserver);
        onAttachmentsChanged();
    }

    public void destroy() {
        mModelList.removeObserver(mListObserver);
        endInput();
    }

    @EnsuresNonNullIf("mInput")
    private boolean isInInputSession() {
        return mInput != null;
    }

    /**
     * Called when the user begins interacting with the Omnibox.
     *
     * @param input The input state for the new session. The input may be replaced without going
     *     through the endInput() (valid -> valid). This is the case for tab switching.
     */
    /* package */ void beginInput(AutocompleteInput input) {
        setAutocompleteInput(input);
        setToolbarVisible(true);
    }

    /** Called when the user stops interacting with the Omnibox. */
    /* package */ void endInput() {
        mModelList.clear();
        setToolbarVisible(false);
        setAutocompleteInput(null);
    }

    private void setAutocompleteInput(@Nullable AutocompleteInput input) {
        if (mInput != null) {
            mInput.getRequestTypeSupplier().removeObserver(mOnAutocompleteRequestTypeChanged);
        }
        mInput = input;

        if (mInput != null) {
            // TODO(crbug.com/481365131): there must be a better way to do that.
            if (mInput.getRequestType() == AutocompleteRequestType.AI_MODE
                    && mInput.getFocusReason() == OmniboxFocusReason.NTP_AI_MODE) {
                activateAiMode(AiModeActivationSource.NTP_BUTTON);
            }

            mInput.getRequestTypeSupplier()
                    .addSyncObserverAndCallIfNonNull(mOnAutocompleteRequestTypeChanged);
        }
    }

    private Snackbar createStyledSnackbar(CharSequence text, int snackbarIdentifier) {
        Snackbar snackbar =
                Snackbar.make(text, null, Snackbar.TYPE_NOTIFICATION, snackbarIdentifier);
        boolean isIncognito = mProfile.isOffTheRecord();
        snackbar.setBackgroundColor(ChromeColors.getInverseBgColor(mContext, isIncognito));

        int textAppearanceResId =
                isIncognito
                        ? org.chromium.components.browser_ui.styles.R.style
                                .TextAppearance_TextMedium_Primary_Baseline_Dark
                        : org.chromium.components.browser_ui.styles.R.style
                                .TextAppearance_TextMedium_OnInverseSurface;
        snackbar.setTextAppearance(textAppearanceResId);
        return snackbar;
    }

    /** Apply a variant of the branded color scheme to Fusebox UI elements */
    /*package */ void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        mModel.set(FuseboxProperties.COLOR_SCHEME, brandedColorScheme);
        mModelList.updateVisualsForState(brandedColorScheme);
    }

    private void onRequestTypeButtonClicked() {
        if (!isInInputSession()) return;

        switch (mInput.getRequestType()) {
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
        if (!isInInputSession()) return;

        if (mInput.getRequestType() == AutocompleteRequestType.SEARCH) return;
        mInput.setRequestType(AutocompleteRequestType.SEARCH);

        mModelList.clear();
    }

    /** Activate AI Mode if no other custom mode is already active. */
    void maybeActivateAiMode(@AiModeActivationSource int activationReason) {
        mPopup.dismiss();
        if (!isInInputSession()) return;

        if (mInput.getRequestType() != AutocompleteRequestType.SEARCH) return;
        activateAiMode(activationReason);
    }

    /** Activate AI Mode as the Next Request fulfillment type. */
    void activateAiMode(@AiModeActivationSource int activationReason) {
        mPopup.dismiss();
        if (!isInInputSession()) return;

        if (mInput.getRequestType() == AutocompleteRequestType.AI_MODE) return;
        FuseboxMetrics.notifyAiModeActivated(activationReason);
        mInput.setRequestType(AutocompleteRequestType.AI_MODE);
    }

    /** Activate image generation as the Next Request fulfillment type. */
    void activateImageGeneration() {
        mPopup.dismiss();
        if (!isInInputSession()) return;

        if (mInput.getRequestType() == AutocompleteRequestType.IMAGE_GENERATION) {
            return;
        }
        mInput.setRequestType(AutocompleteRequestType.IMAGE_GENERATION);
    }

    /**
     * Show or hide the Fusebox toolbar.
     *
     * @param visible Whether the toolbar should be visible.
     */
    void setToolbarVisible(boolean visible) {
        if (!isInInputSession()) return;

        mModel.set(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE, visible);
        // The omnibox can become focused with the autocomplete request type immediately set
        // to AI_MODE. We check the mode here to avoid erroneously staying in compact mode.
        setUseCompactUi(
                OmniboxFeatures.sCompactFusebox.getValue()
                        && mInput.getRequestType() == AutocompleteRequestType.SEARCH);
    }

    /**
     * @param url The search URL to get the AIM analog of.
     * @param callback The callback to run with the URL for the AIM service.
     */
    void getAimUrl(GURL url, Callback<GURL> callback) {
        mComposeboxQueryControllerBridge.getAimUrl(url, callback);
    }

    /**
     * @param url The search URL to get the Image generator analog of.
     * @param callback The callback to run with the URL for the image generation service.
     */
    void getImageGenerationUrl(GURL url, Callback<GURL> callback) {
        mComposeboxQueryControllerBridge.getImageGenerationUrl(url, callback);
    }

    @VisibleForTesting
    void onToggleAttachmentsPopup() {
        if (mPopup.isShowing()) {
            mPopup.dismiss();
        } else {
            updateModelForCurrentTab();
            mModel.set(
                    FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE,
                    Clipboard.getInstance().hasImage());
            mPopup.show();
        }

        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        FuseboxMetrics.notifyAttachmentsPopupToggled(!mPopup.isShowing(), mModel, tracker);
    }

    private void updateModelForCurrentTab() {
        var tabSelector = mTabModelSelectorSupplier.get();
        var shouldShowCurrentTab =
                tabSelector != null
                        && tabSelector.getCurrentTab() != null
                        && !mModelList
                                .getAttachedTabIds()
                                .contains(tabSelector.getCurrentTab().getId())
                        && OmniboxFeatures.sAllowCurrentTab.getValue();

        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE, shouldShowCurrentTab);
        if (!shouldShowCurrentTab) return;

        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        assumeNonNull(tabModelSelector);
        Tab currentTab = assumeNonNull(tabModelSelector.getCurrentTab());
        boolean tabIsEligible =
                FuseboxTabUtils.isTabEligibleForAttachment(currentTab)
                        && FuseboxTabUtils.isTabActive(currentTab);

        if (tabIsEligible) {
            mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE, true);
            mModel.set(
                    FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_CLICKED,
                    () -> onAddCurrentTab(currentTab));
            mModel.set(
                    FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_FAVICON,
                    OmniboxResourceProvider.getFaviconBitmapForTab(currentTab));
        } else {
            mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_VISIBLE, false);
        }
    }

    private void onAddCurrentTab(Tab tab) {
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CURRENT_TAB);
        maybeActivateAiMode(AiModeActivationSource.IMPLICIT);

        Set<Integer> currentAttachedIds = mModelList.getAttachedTabIds();
        if (currentAttachedIds.contains(tab.getId())) return;
        var attachment =
                FuseboxAttachment.forTab(
                        tab, mContext.getResources(), FuseboxAttachmentButtonType.CURRENT_TAB);

        // Use FuseboxModelList's add method which handles upload automatically
        mModelList.add(attachment);
    }

    /**
     * Check whether additional attachments of a specific kind are allowed, showing a snackbar when
     * limit is reached.
     */
    @VisibleForTesting
    /* package */ boolean isMaxAttachmentCountReached(@FuseboxAttachmentType int attachmentType) {
        if (!isInInputSession()) return true;

        boolean isImageGenerationUsed =
                mInput.getRequestType() == AutocompleteRequestType.IMAGE_GENERATION;

        // Permit image reselection when image generation is picked.
        if (attachmentType == FuseboxAttachmentType.ATTACHMENT_IMAGE && isImageGenerationUsed) {
            return false;
        }

        // Permit tab reselection (except image generation).
        if (attachmentType == FuseboxAttachmentType.ATTACHMENT_TAB
                && !isImageGenerationUsed
                && !mModelList.getAttachedTabIds().isEmpty()) {
            return false;
        }

        // Permit additional attachments, except when creating images.
        if (mModelList.getRemainingAttachments() > 0 && !isImageGenerationUsed) {
            return false;
        }
        return true;
    }

    private void onAttachmentsChanged() {
        mModel.set(FuseboxProperties.ATTACHMENTS_VISIBLE, !mModelList.isEmpty());
        mModel.set(
                FuseboxProperties.POPUP_TOOL_CREATE_IMAGE_ENABLED,
                areAttachmentsCompatibleWithCreateImage());
        updatePopupButtonEnabledStates();
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
        if (isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB)) return;

        Intent intent;
        ArrayList<Integer> preselectedTabIds = new ArrayList<>(mModelList.getAttachedTabIds());
        try {
            intent =
                    new Intent(
                                    mContext,
                                    Class.forName(
                                            ChromeItemPickerExtras
                                                    .CHROME_ITEM_PICKER_ACTIVITY_CLASS))
                            .putIntegerArrayListExtra(
                                    ChromeItemPickerExtras.EXTRA_PRESELECTED_TAB_IDS,
                                    preselectedTabIds);
            ProfileIntentUtils.addProfileToIntent(mProfile, intent);

            TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
            boolean isIncognitoBrandedModelSelected = false;
            if (tabModelSelector != null) {
                isIncognitoBrandedModelSelected =
                        tabModelSelector.isIncognitoBrandedModelSelected();
            }
            intent.putExtra(
                    ChromeItemPickerExtras.EXTRA_IS_INCOGNITO_BRANDED,
                    isIncognitoBrandedModelSelected);
        } catch (ClassNotFoundException e) {
            return;
        }

        int maxAllowedTabs = preselectedTabIds.size() + remainingAttachments;
        intent.putExtra(ChromeItemPickerExtras.EXTRA_ALLOWED_SELECTION_COUNT, maxAllowedTabs);

        boolean isSingleContextMode = !OmniboxFeatures.sMultiattachmentFusebox.getValue();
        intent.putExtra(ChromeItemPickerExtras.EXTRA_IS_SINGLE_CONTEXT_MODE, isSingleContextMode);

        mWindowAndroid.showCancelableIntent(
                intent, this::onTabPickerResult, R.string.low_memory_error);
    }

    void onTabPickerResult(int resultCode, @Nullable Intent data) {
        if (resultCode != Activity.RESULT_OK || data == null || data.getExtras() == null) return;
        ArrayList<Integer> tabIds =
                data.getIntegerArrayListExtra(ChromeItemPickerExtras.EXTRA_ATTACHMENT_TAB_IDS);
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
        try (var batchToken = mModelList.beginBatchEdit()) {
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
                    if (tab == null) continue;
                    boolean addFailed =
                            !mModelList.add(
                                    FuseboxAttachment.forTab(
                                            tab,
                                            mContext.getResources(),
                                            FuseboxAttachmentButtonType.TAB_PICKER));
                    if (addFailed) {
                        break;
                    }
                }
            }
        }
    }

    @VisibleForTesting
    void onCameraClicked() {
        mPopup.dismiss();
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CAMERA);
        if (isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_IMAGE)) return;

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
        updatePopupButtonEnabledStates();
    }

    private void updatePopupButtonEnabledStates() {
        if (!isInInputSession()) return;

        // Disable Camera and Gallery Selection popup buttons if no remaining attachments are left.
        boolean allowByCapacity = mModelList.getRemainingAttachments() > 0;

        // Disables popup buttons for Current Tab, Tab Picker, and File selection if the
        // autocomplete request is not image generation and if there are no remaining attachments.
        boolean allowNonImage =
                mInput.getRequestType() != AutocompleteRequestType.IMAGE_GENERATION
                        && allowByCapacity;

        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED, allowNonImage);
        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED, allowNonImage);
        mModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED, allowNonImage);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED, allowByCapacity);
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED, allowByCapacity);
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

                    long startTime = SystemClock.elapsedRealtime();
                    ByteArrayOutputStream byteArrayOutputStream = new ByteArrayOutputStream();
                    bitmap.compress(CompressFormat.PNG, 100, byteArrayOutputStream);
                    byte[] dataBytes = byteArrayOutputStream.toByteArray();
                    var attachment =
                            FuseboxAttachment.forImage(
                                    new BitmapDrawable(mContext.getResources(), bitmap),
                                    /* title= */ "",
                                    "image/png",
                                    dataBytes,
                                    startTime,
                                    FuseboxAttachmentButtonType.CAMERA);
                    uploadAndAddAttachment(attachment);
                },
                R.string.low_memory_error);
    }

    @VisibleForTesting
    void onImagePickerClicked() {
        mPopup.dismiss();
        if (!isInInputSession()) return;

        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.GALLERY);
        if (isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_IMAGE)) return;

        boolean allowMultipleAttachments =
                mInput.getRequestType() != AutocompleteRequestType.IMAGE_GENERATION;
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

                    try (var batchToken = mModelList.beginBatchEdit()) {
                        var uris = extractUrisFromResult(data);
                        for (var uri : uris) {
                            fetchAttachmentDetails(
                                    uri,
                                    FuseboxAttachmentType.ATTACHMENT_IMAGE,
                                    this::uploadAndAddAttachment,
                                    FuseboxAttachmentButtonType.GALLERY);
                        }
                    }
                },
                R.string.low_memory_error);
    }

    @VisibleForTesting
    void onFilePickerClicked() {
        mPopup.dismiss();
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.FILES);
        if (isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_FILE)) return;

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

                    try (var batchToken = mModelList.beginBatchEdit()) {
                        var uris = extractUrisFromResult(data);
                        for (var uri : uris) {
                            fetchAttachmentDetails(
                                    uri,
                                    FuseboxAttachmentType.ATTACHMENT_FILE,
                                    this::uploadAndAddAttachment,
                                    FuseboxAttachmentButtonType.FILES);
                        }
                    }
                },
                /* errorId= */ android.R.string.cancel);
    }

    @VisibleForTesting
    void onClipboardClicked() {
        mPopup.dismiss();
        FuseboxMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CLIPBOARD);
        if (isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_IMAGE)) return;

        long startTime = SystemClock.elapsedRealtime();
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
                        FuseboxAttachment.forImage(
                                new BitmapDrawable(mContext.getResources(), bitmap),
                                /* title= */ "",
                                "image/png",
                                pngBytes,
                                startTime,
                                FuseboxAttachmentButtonType.CLIPBOARD);
                uploadAndAddAttachment(attachment);
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    @VisibleForTesting
    void fetchAttachmentDetails(
            Uri uri,
            @FuseboxAttachmentType int type,
            Callback<FuseboxAttachment> callback,
            @FuseboxAttachmentButtonType int buttonType) {
        new FuseboxAttachmentDetailsFetcher(
                        mContext, mContext.getContentResolver(), uri, type, callback, buttonType)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Add an attachment to the Fusebox toolbar.
     *
     * @param attachment Contains information about the input that will be added as context.
     */
    /* package */ void uploadAndAddAttachment(FuseboxAttachment attachment) {
        if (!isInInputSession()) return;

        // Image generation is only allowed to have a single piece of context.
        if (mInput.getRequestType() == AutocompleteRequestType.IMAGE_GENERATION) {
            mModelList.clear();
        }

        // Use FuseboxModelList's unified add method.
        mModelList.add(attachment);
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

    void setUseCompactUi(boolean useCompactUi) {
        boolean fuseboxActive = mModel.get(FuseboxProperties.ATTACHMENTS_TOOLBAR_VISIBLE);
        mFuseboxStateSupplier.set(
                fuseboxActive
                        ? useCompactUi ? FuseboxState.COMPACT : FuseboxState.EXPANDED
                        : FuseboxState.DISABLED);
        mModel.set(FuseboxProperties.COMPACT_UI, useCompactUi);
    }
}
