// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import static org.chromium.build.NullUtil.assertNonNull;
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
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.FuseboxSessionState;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentModelList.FuseboxAttachmentChangeListener;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxAttachmentRecyclerViewAdapter.FuseboxAttachmentType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.AiModeActivationSource;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxMetrics.FuseboxAttachmentButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonData;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupButtonType;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxProperties.PopupState;
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
import org.chromium.components.browser_ui.util.ChromeItemPickerUtils;
import org.chromium.components.browser_ui.widget.scrim.ScrimManager;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.contextual_search.InputState;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.metrics.OmniboxEventProtos.OmniboxEventProto.PageClassification;
import org.chromium.components.omnibox.AutocompleteInput;
import org.chromium.components.omnibox.AutocompleteRequestType;
import org.chromium.components.omnibox.IconResourceIdsProto.IconResourceIds;
import org.chromium.components.omnibox.InputTypeProto.InputType;
import org.chromium.components.omnibox.ModelConfigProto.ModelConfig;
import org.chromium.components.omnibox.OmniboxFeatures;
import org.chromium.components.omnibox.OmniboxFocusReason;
import org.chromium.components.omnibox.ToolConfigProto.ToolConfig;
import org.chromium.components.omnibox.ToolModeProto.ToolMode;
import org.chromium.components.omnibox.ToolModeUtils;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.AndroidPermissionDelegate;

import java.io.ByteArrayOutputStream;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.function.Supplier;

/** Mediator for the Fusebox component. */
@NullMarked
/* package */ class FuseboxMediator implements FuseboxAttachmentChangeListener {
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final AndroidPermissionDelegate mPermissionDelegate;
    private final PropertyModel mModel;
    private final FuseboxViewHolder mViewHolder;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;
    private final SettableNonNullObservableSupplier<@FuseboxState Integer> mFuseboxStateSupplier;
    private final Clipboard mClipboard;
    private final Callback<@AutocompleteRequestType Integer> mOnAutocompleteRequestTypeChanged =
            this::onAutocompleteRequestTypeChanged;
    private final Callback<InputState> mOnInputStateChanged = this::onInputStateChange;
    private final Callback<List<SuggestedTabInfo>> mOnSuggestedTabsChanged =
            this::reconcileSuggestedTabs;
    private final SnackbarManager mSnackbarManager;
    private final Snackbar mAttachmentUploadFailedSnackbar;
    private final ScrimManager mScrimManager;
    private final Supplier<@Nullable View> mScrimAnchorViewSupplier;

    private boolean mIsTextWrapping;
    private boolean mHasContextualTasksFocus;
    private @BrandedColorScheme int mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;
    private @Nullable Profile mProfile;
    private @Nullable AutocompleteInput mInput;
    private @Nullable FuseboxAttachmentModelList mModelList;
    private @Nullable ComposeboxQueryControllerBridge mComposeboxQueryControllerBridge;
    private @Nullable FuseboxMetrics mMetrics;
    private @Nullable PropertyModel mScrimModel;
    private boolean mActionTaken;

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

    /* package */ FuseboxMediator(
            Context context,
            WindowAndroid windowAndroid,
            PropertyModel model,
            FuseboxViewHolder viewHolder,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier,
            SettableNonNullObservableSupplier<@FuseboxState Integer> fuseboxStateSupplier,
            SettableNonNullObservableSupplier<@FuseboxLayoutMode Integer> fuseboxLayoutModeSupplier,
            SnackbarManager snackbarManager,
            Clipboard clipboard,
            ScrimManager scrimManager,
            Supplier<@Nullable View> scrimAnchorViewSupplier) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mPermissionDelegate = windowAndroid;
        mModel = model;
        mViewHolder = viewHolder;
        mViewHolder.popup.addOnDismissListener(this::hidePopup);
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
        mFuseboxStateSupplier = fuseboxStateSupplier;
        mSnackbarManager = snackbarManager;
        mClipboard = clipboard;
        mScrimManager = scrimManager;
        mScrimAnchorViewSupplier = scrimAnchorViewSupplier;

        // Create the upload failed snackbar.
        mAttachmentUploadFailedSnackbar =
                Snackbar.make(
                        context.getText(R.string.fusebox_upload_failed),
                        /* controller= */ null,
                        Snackbar.TYPE_NOTIFICATION,
                        Snackbar.UMA_FUSEBOX_UPLOAD_FAILED);

        fuseboxLayoutModeSupplier.set(getFuseboxLayoutMode());

        mModel.set(FuseboxProperties.BUTTON_ADD_CLICKED, this::onPlusButtonClicked);
        mModel.set(
                FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE_CLICKED,
                this::onRequestTypeButtonClicked);

        mModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_CLICKED, this::onTabPickerClicked);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_CLICKED, this::onClipboardClicked);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_CLICKED, this::onCameraClicked);
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_CLICKED, this::onImagePickerClicked);
        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_CLICKED, this::onFilePickerClicked);
        mModel.set(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST, List.of());
        mModel.set(FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST, List.of());

        mModel.set(
                FuseboxProperties.POPUP_ATTACH_TAB_PICKER_VISIBLE,
                ChromeFeatureList.sChromeItemPickerUi.isEnabled());
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_VISIBLE, true);
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_VISIBLE, true);
        mModel.set(FuseboxProperties.POPUP_TOOL_DIVIDER_VISIBLE, true);
        mModel.set(
                FuseboxProperties.POPUP_TOOL_HEADER_VISIBLE,
                OmniboxFeatures.sShowModelPicker.getValue());

        mModel.set(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE, false);
        mModel.set(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE, false);
    }

    /* package */ void destroy() {
        endInput();
    }

    public boolean wasActionTaken() {
        return mActionTaken;
    }

    @EnsuresNonNullIf(
            value = {
                "mProfile",
                "mInput",
                "mModelList",
                "mComposeboxQueryControllerBridge",
                "mMetrics"
            })
    private boolean isInInputSession() {
        return mProfile != null
                && mInput != null
                && mModelList != null
                && mComposeboxQueryControllerBridge != null
                && mMetrics != null;
    }

    private void setController(@Nullable ComposeboxQueryControllerBridge controller) {
        if (mComposeboxQueryControllerBridge != null) {
            if (OmniboxFeatures.sShowModelPicker.getValue()) {
                mComposeboxQueryControllerBridge
                        .getInputStateSupplier()
                        .removeObserver(mOnInputStateChanged);
            }
            mComposeboxQueryControllerBridge
                    .getSuggestedTabsSupplier()
                    .removeObserver(mOnSuggestedTabsChanged);
        }

        mComposeboxQueryControllerBridge = controller;
        if (mComposeboxQueryControllerBridge == null) return;

        if (OmniboxFeatures.sShowModelPicker.getValue()) {
            mComposeboxQueryControllerBridge
                    .getInputStateSupplier()
                    .addSyncObserverAndCallIfNonNull(mOnInputStateChanged);
        }

        mComposeboxQueryControllerBridge
                .getSuggestedTabsSupplier()
                .addSyncObserver(mOnSuggestedTabsChanged);

        mModel.set(
                FuseboxProperties.POPUP_ATTACH_FILE_VISIBLE,
                mComposeboxQueryControllerBridge.isPdfUploadEligible());
        if (!OmniboxFeatures.sShowModelPicker.getValue() && mProfile != null) {
            updateClientControlledToolButtonList();
        }
    }

    private void setModelList(@Nullable FuseboxAttachmentModelList modelList) {
        if (mModelList == modelList) return;

        if (mModelList != null) {
            mModelList.removeObserver(mListObserver);
            mModelList.removeAttachmentChangeListener(this);
            mModelList.setAttachmentUploadFailedListener(null);
        }

        mModelList = modelList;

        if (mModelList != null) {
            var adapter = mModelList.getAdapter();
            mViewHolder.attachmentsView.setAdapter(adapter);
            mModel.set(FuseboxProperties.ADAPTER, adapter);
            mModelList.setAttachmentUploadFailedListener(this::onAttachmentUploadFailed);
            mModelList.updateVisualsForState(mBrandedColorScheme);
            mModelList.addAttachmentChangeListener(this);
            mModelList.addObserver(mListObserver);
            onAttachmentsChanged();
        } else {
            // need a safe fallback.
            mViewHolder.attachmentsView.setAdapter(null);
            mModel.set(FuseboxProperties.ADAPTER, null);
            mModel.set(FuseboxProperties.ATTACHMENTS_VISIBLE, false);
        }
    }

    /**
     * Called when the user begins interacting with the Omnibox.
     *
     * @param session The input state for the new session. The input may be replaced without going
     *     through the endInput() (valid -> valid). This is the case for tab switching.
     */
    /* package */ void beginInput(FuseboxSessionState session) {
        mActionTaken = false;
        mMetrics = session.getMetrics();
        mProfile = assertNonNull(session.getProfile());
        setController(session.getComposeboxQueryControllerBridge());
        setModelList(session.getFuseboxAttachmentModelList());
        setAutocompleteInput(session.getAutocompleteInput());
        onAttachmentsChanged();
        updateFuseboxState();
        updateSnackbarStyling();
    }

    /**
     * Called when the user stops interacting with the Omnibox.
     *
     * <p>For standard search, this is called on every focus loss to clear the UI. For Contextual
     * Tasks, this is only called when the task is destroyed (e.g., tab switch or explicit close) to
     * keep the session warm during focus loss.
     */
    /* package */ void endInput() {
        hidePopup();
        setModelList(null);
        setController(null);
        setAutocompleteInput(null);
        mProfile = null;
        mMetrics = null;
        mIsTextWrapping = false;
        updateFuseboxState();
    }

    /**
     * Called when focus is lost or gained while in a Contextual Tasks session.
     *
     * @param hasFocus Whether the contextual tasks fusebox has focus.
     */
    /* package */ void onContextualTaskFocusChanged(boolean hasFocus) {
        if (mHasContextualTasksFocus == hasFocus) return;
        mHasContextualTasksFocus = hasFocus;

        if (!isInInputSession()) return;

        if (!hasFocus) {
            hidePopup();
            mIsTextWrapping = false;
        }
        updateFuseboxState();
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
                FuseboxMetrics.notifyAiModeActivated(AiModeActivationSource.NTP_BUTTON);
            } else if (mInput.getFocusReason() == OmniboxFocusReason.FAKE_BOX_PLUS_BUTTON_TAP) {
                showPopup();
            }

            mInput.getRequestTypeSupplier()
                    .addSyncObserverAndCallIfNonNull(mOnAutocompleteRequestTypeChanged);
        }
    }

    private void updateSnackbarStyling() {
        boolean isIncognito = mProfile != null && mProfile.isOffTheRecord();
        mAttachmentUploadFailedSnackbar.setBackgroundColor(
                ChromeColors.getInverseBgColor(mContext, isIncognito));

        int textAppearanceResId =
                isIncognito
                        ? R.style.TextAppearance_TextMedium_Primary_Baseline_Dark
                        : R.style.TextAppearance_TextMedium_OnInverseSurface;
        mAttachmentUploadFailedSnackbar.setTextAppearance(textAppearanceResId);
    }

    /** Apply a variant of the branded color scheme to Fusebox UI elements */
    /* package */ void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        mBrandedColorScheme = brandedColorScheme;
        mModel.set(FuseboxProperties.COLOR_SCHEME, brandedColorScheme);
        if (mModelList == null) return;
        mModelList.updateVisualsForState(brandedColorScheme);
    }

    private void onRequestTypeButtonClicked() {
        if (!isInInputSession()) return;

        // On Desktop, dedicated button shows in specialized modes only, and reverts to AI Mode.
        if (ToolModeUtils.isAimRequest(mInput.getRequestType())
                && !OmniboxFeatures.isDesktopPlatform()) {
            activateSearchMode();
        } else {
            activateAiMode(
                    AutocompleteRequestType.AI_MODE, AiModeActivationSource.DEDICATED_BUTTON);
        }
    }

    private void activateSearchMode() {
        if (trySetRequestType(AutocompleteRequestType.SEARCH)) {
            assert mModelList != null;
            mModelList.clear();
            if (OmniboxFeatures.sShowModelPicker.getValue()
                    && mComposeboxQueryControllerBridge != null) {
                InputState inputState =
                        mComposeboxQueryControllerBridge.getInputStateSupplier().get();
                if (inputState != null) {
                    mComposeboxQueryControllerBridge.setActiveModel(inputState.defaultModel);
                }
            }
        }
    }

    private void maybeActivateAiMode(@AiModeActivationSource int activationReason) {
        if (!isInInputSession()) return;

        hidePopup();
        if (mInput.getRequestType() != AutocompleteRequestType.SEARCH) return;

        activateAiMode(AutocompleteRequestType.AI_MODE, activationReason);
    }

    private void activateAiMode(
            @AutocompleteRequestType int requestType,
            @AiModeActivationSource int activationReason) {
        assert ToolModeUtils.isAimRequest(requestType);
        if (!isInInputSession()) return;
        boolean wasConventional = ToolModeUtils.isConventionalRequest(mInput.getRequestType());
        if (trySetRequestType(requestType) && wasConventional) {
            FuseboxMetrics.notifyAiModeActivated(activationReason);
        }
    }

    /* package */ void setIsTextWrapping(boolean isTextWrapping) {
        mIsTextWrapping = isTextWrapping;
        updateFuseboxState();
    }

    private void updateFuseboxState() {
        @FuseboxState int targetState;
        boolean showRequestTypeButton = shouldShowRequestTypeButton();
        boolean isContextualTasks =
                mInput != null
                        && mInput.getRawPageClassification()
                                == PageClassification.CO_BROWSING_COMPOSEBOX_VALUE;

        if (!isInInputSession()) {
            targetState = FuseboxState.DISABLED;
        } else if (!mHasContextualTasksFocus && isContextualTasks) {
            targetState = FuseboxState.COMPACT;
        } else if (!OmniboxFeatures.sCompactFusebox.getValue()) {
            targetState = FuseboxState.EXPANDED;
        } else {
            targetState =
                    // If we're showing the request type button...
                    showRequestTypeButton
                                    // or the text is wrapping...
                                    || mIsTextWrapping
                                    // or the attachments list has elements - expand the fusebox.
                                    || !mModelList.isEmpty()
                            ? FuseboxState.EXPANDED
                            : FuseboxState.COMPACT;
        }
        mFuseboxStateSupplier.set(targetState);
        mModel.set(FuseboxProperties.FUSEBOX_STATE, targetState);
        mModel.set(FuseboxProperties.ADD_BUTTON_VISIBLE, targetState == FuseboxState.EXPANDED);
        mModel.set(FuseboxProperties.SHOW_REQUEST_TYPE_BUTTON, showRequestTypeButton);
    }

    @SuppressWarnings("checkstyle:SimplifyBooleanReturn")
    private boolean shouldShowRequestTypeButton() {
        // Conditions here, when grouped into a single return, are difficult to parse.
        // Breaking down into explicit if/elseif/else to help understand what's going on.
        if (!isInInputSession()) {
            return false;
        } else if (ToolModeUtils.isConventionalRequest(mInput.getRequestType())) {
            // Never show mode button if in Search mode.
            return false;
        } else if (mInput.getRequestType() == AutocompleteRequestType.AI_MODE
                && OmniboxFeatures.isDesktopPlatform()) {
            // Special Desktop case -> AI Mode only changes the status icon.
            return false;
        }

        return true;
    }

    /** Toggles the visibility of the attachments popup. */
    /* package */ void onPlusButtonClicked() {
        if (!isInInputSession()) return;

        if (mModel.get(FuseboxProperties.POPUP_STATE) != PopupState.HIDDEN) {
            hidePopup();
        } else {
            showPopup();
        }

        Tracker tracker = TrackerFactory.getTrackerForProfile(mProfile);
        mMetrics.notifyAttachmentsPopupToggled(
                mModel.get(FuseboxProperties.POPUP_STATE) != PopupState.HIDDEN, mModel, tracker);
    }

    private void showPopup() {
        if (!isInInputSession()) return;
        updateModelForCurrentTab();
        mModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_VISIBLE, mClipboard.hasImage());
        mModel.set(
                FuseboxProperties.POPUP_STATE,
                OmniboxFeatures.sShowBottomSheetPopup.getValue()
                        ? PopupState.BOTTOM
                        : PopupState.FLOATING);
        if (mScrimManager != null
                && mModel.get(FuseboxProperties.POPUP_STATE) == PopupState.BOTTOM) {
            View scrimAnchor = mScrimAnchorViewSupplier.get();
            if (scrimAnchor == null) {
                scrimAnchor = mViewHolder.parentView;
            }
            mScrimModel =
                    new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                            .with(ScrimProperties.ANCHOR_VIEW, scrimAnchor)
                            .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, true)
                            .with(ScrimProperties.CLICK_DELEGATE, this::hidePopup)
                            .with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                            .build();
            mScrimManager.showScrim(mScrimModel);
        }
    }

    /** Hides the popup if currently shown */
    /* package */ boolean handleHidePopup() {
        if (mModel.get(FuseboxProperties.POPUP_STATE) == PopupState.HIDDEN) {
            return false;
        } else {
            hidePopup();
            return true;
        }
    }

    private void hidePopup() {
        mModel.set(FuseboxProperties.POPUP_STATE, PopupState.HIDDEN);
        if (mScrimModel != null) {
            mScrimManager.hideScrim(mScrimModel, /* animate= */ true);
        }
    }

    private void updateModelForCurrentTab() {
        if (!isInInputSession()) return;
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
        if (!isInInputSession()) return;
        mMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CURRENT_TAB);
        maybeActivateAiMode(AiModeActivationSource.IMPLICIT);

        Set<Integer> currentAttachedIds = mModelList.getAttachedTabIds();
        if (currentAttachedIds.contains(tab.getId())) return;
        var attachment =
                FuseboxAttachment.forTab(
                        tab,
                        isCurrentTab(tab),
                        mContext.getResources(),
                        FuseboxAttachmentButtonType.CURRENT_TAB,
                        /* isSuggestedTab= */ false);

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
        if ((attachmentType == FuseboxAttachmentType.ATTACHMENT_IMAGE
                        || attachmentType == FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL)
                && isImageGenerationUsed) {
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
        if (!isInInputSession()) return;
        mModel.set(FuseboxProperties.ATTACHMENTS_VISIBLE, !mModelList.isEmpty());

        if (!OmniboxFeatures.sShowModelPicker.getValue()) {
            updateClientControlledToolButtonList();
            updatePopupButtonEnabledStates();
        }
    }

    private boolean areAttachmentsCompatibleWithCreateImage() {
        if (!isInInputSession()) return false;
        int imageCount = 0;
        for (MVCListAdapter.ListItem listItem : mModelList) {
            if (listItem.type == FuseboxAttachmentType.ATTACHMENT_FILE) {
                return false;
            }
            if (listItem.type == FuseboxAttachmentType.ATTACHMENT_TAB) {
                return false;
            }
            if (listItem.type == FuseboxAttachmentType.ATTACHMENT_PDF) {
                return false;
            }
            if (listItem.type == FuseboxAttachmentType.ATTACHMENT_IMAGE
                    || listItem.type == FuseboxAttachmentType.ATTACHMENT_IMAGE_NO_THUMBNAIL) {
                imageCount++;
            }
        }
        return imageCount <= 1;
    }

    private void onTabPickerClicked() {
        if (!isInInputSession()) return;

        mActionTaken = true;
        hidePopup();
        mMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.TAB_PICKER);
        if (isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_TAB)) return;

        Intent intent = ChromeItemPickerUtils.createChromeItemPickerIntent(mContext);
        if (intent == null) return;
        ProfileIntentUtils.addProfileToIntent(mProfile, intent);

        ArrayList<Integer> tabIds = new ArrayList<>(mModelList.getAttachedTabIds());
        intent.putIntegerArrayListExtra(ChromeItemPickerExtras.EXTRA_PRESELECTED_TAB_IDS, tabIds);

        TabModelSelector selector = mTabModelSelectorSupplier.get();
        boolean isIncognito = selector != null && selector.isIncognitoBrandedModelSelected();
        intent.putExtra(ChromeItemPickerExtras.EXTRA_IS_INCOGNITO_BRANDED, isIncognito);

        int maxAllowedTabs = tabIds.size() + mModelList.getRemainingAttachments();
        intent.putExtra(ChromeItemPickerExtras.EXTRA_ALLOWED_SELECTION_COUNT, maxAllowedTabs);

        boolean isSingleContextMode = !OmniboxFeatures.sMultiattachmentFusebox.getValue();
        intent.putExtra(ChromeItemPickerExtras.EXTRA_IS_SINGLE_CONTEXT_MODE, isSingleContextMode);

        mWindowAndroid.showCancelableIntent(
                intent, this::onTabPickerResult, R.string.low_memory_error);
    }

    @VisibleForTesting
    /* package */ void onTabPickerResult(int resultCode, @Nullable Intent data) {
        if (!isInInputSession()) return;

        if (resultCode == Activity.RESULT_CANCELED) {
            if (data != null && data.hasExtra(ChromeItemPickerExtras.EXTRA_ITEM_PICKER_ERROR)) {
                onAttachmentUploadFailed();
            }
            return;
        }

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

    @VisibleForTesting
    /* package */ void onAttachmentUploadFailed() {
        mSnackbarManager.showSnackbar(mAttachmentUploadFailedSnackbar);
    }

    /**
     * Reconciles the model list attachments with a new set of selected tab IDs by removing
     * deselected tabs and adding newly selected tabs in one pass.
     *
     * @param newlySelectedTabIds The set of Tab IDs (as Integer) that are now selected by the user.
     */
    @VisibleForTesting
    /* package */ void updateCurrentlyAttachedTabs(Set<Integer> newlySelectedTabIds) {
        if (!isInInputSession()) return;
        TabModelSelector tabModelSelector = mTabModelSelectorSupplier.get();
        if (tabModelSelector == null) return;

        Set<Integer> currentAttachedIds = mModelList.getAttachedTabIds();
        try (var ignored = mModelList.beginBatchEdit()) {
            mModelList.removeTabsNotInSet(newlySelectedTabIds);

            for (int id : newlySelectedTabIds) {
                if (!currentAttachedIds.contains(id)) {
                    Tab tab = tabModelSelector.getTabById(id);
                    if (tab == null) continue;
                    boolean addFailed =
                            !mModelList.add(
                                    FuseboxAttachment.forTab(
                                            tab,
                                            isCurrentTab(tab),
                                            mContext.getResources(),
                                            FuseboxAttachmentButtonType.TAB_PICKER,
                                            /* isSuggestedTab= */ false));
                    if (addFailed) {
                        break;
                    }
                }
            }
        }
    }

    private void reconcileSuggestedTabs(List<SuggestedTabInfo> suggestedTabs) {
        if (!isInInputSession()) return;
        if (mModelList == null) return;

        // First, clear any existing suggested chips.
        mModelList.removeSuggestedTabs();

        if (suggestedTabs.isEmpty()) return;

        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return;

        Set<Integer> attachedTabIds = mModelList.getAttachedTabIds();
        for (SuggestedTabInfo info : suggestedTabs) {
            if (!attachedTabIds.contains(info.tabId)) {
                Tab tab = selector.getTabById(info.tabId);
                if (tab == null) continue;

                if (mModelList.getRemainingAttachments() == 0) break;

                var attachment =
                        FuseboxAttachment.forTab(
                                tab,
                                /* bypassTabCache= */ false,
                                mContext.getResources(),
                                FuseboxAttachmentButtonType.SUGGESTED_TAB,
                                /* isSuggestedTab= */ true);
                attachment.setUploadIsComplete();
                mModelList.add(attachment);
                mMetrics.notifyAttachmentButtonShown(FuseboxAttachmentButtonType.SUGGESTED_TAB);
            }
        }
    }

    private void onCameraClicked() {
        if (!isInInputSession()) return;

        mActionTaken = true;
        hidePopup();
        mMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CAMERA);
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
        updateFuseboxState();
        mModel.set(FuseboxProperties.AUTOCOMPLETE_REQUEST_TYPE, type);

        if (type != AutocompleteRequestType.AI_MODE && isInInputSession() && mModelList != null) {
            mModelList.removeSuggestedTabs();
        }

        if (OmniboxFeatures.sShowModelPicker.getValue()) {
            if (!isInInputSession()) return;
            InputState inputState = mComposeboxQueryControllerBridge.getInputStateSupplier().get();
            if (inputState != null) {
                onInputStateChange(inputState);
            }
        } else {
            updateClientControlledToolButtonList();
            updatePopupButtonEnabledStates();
        }
    }

    private void updatePopupButtonEnabledStates() {
        assert !OmniboxFeatures.sShowModelPicker.getValue();
        if (!isInInputSession()) return;

        // Disable Camera and Gallery Selection popup buttons if no remaining attachments are left.
        boolean allowByCapacity = mModelList.getRemainingAttachments() > 0;

        // Disables popup buttons for Current Tab, Tab Picker, and File selection if the
        // autocomplete request is not image generation and if there are no remaining attachments.
        boolean allowNonImage =
                mInput.getRequestType() != AutocompleteRequestType.IMAGE_GENERATION
                        && allowByCapacity;

        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED, allowNonImage);
        mModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED, allowNonImage);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED, allowByCapacity);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED, allowByCapacity);
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED, allowByCapacity);
        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED, allowNonImage);
    }

    private void updateClientControlledToolButtonList() {
        assert !OmniboxFeatures.sShowModelPicker.getValue();
        if (!isInInputSession()) return;
        List<PopupButtonData> toolButtons = new ArrayList<>();

        toolButtons.add(
                new PopupButtonData(
                        this::onDynamicButtonClicked,
                        mContext.getString(R.string.ai_mode_entrypoint_label),
                        R.drawable.search_spark_black_24dp,
                        /* enabled= */ true,
                        mInput.getRequestType() == AutocompleteRequestType.AI_MODE,
                        PopupButtonType.TOOL,
                        ToolMode.TOOL_MODE_UNSPECIFIED_VALUE,
                        /* hasColor= */ false));

        if (mComposeboxQueryControllerBridge.isCreateImagesEligible()) {
            toolButtons.add(
                    new PopupButtonData(
                            this::onDynamicButtonClicked,
                            mContext.getString(R.string.omnibox_create_image),
                            R.drawable.create_image_24dp,
                            areAttachmentsCompatibleWithCreateImage(),
                            mInput.getRequestType() == AutocompleteRequestType.IMAGE_GENERATION,
                            PopupButtonType.TOOL,
                            ToolMode.TOOL_MODE_IMAGE_GEN_VALUE,
                            /* hasColor= */ true));
        }

        mModel.set(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST, toolButtons);
    }

    private void launchCamera() {
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

    private void onImagePickerClicked() {
        if (!isInInputSession()) return;

        mActionTaken = true;
        hidePopup();
        mMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.GALLERY);
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
                    if (!isInInputSession()) return;
                    if (resultCode != Activity.RESULT_OK || data == null) return;

                    try (var batchToken = mModelList.beginBatchEdit()) {
                        var uris = extractUrisFromResult(data);
                        for (var uri : uris) {
                            fetchAttachmentDetails(
                                    uri,
                                    this::uploadAndAddAttachment,
                                    FuseboxAttachmentButtonType.GALLERY);
                        }
                    }
                },
                R.string.low_memory_error);
    }

    private void onFilePickerClicked() {
        if (!isInInputSession()) return;

        mActionTaken = true;
        hidePopup();
        mMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.FILES);
        if (isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_FILE)) return;

        String mimeType =
                ChromeFeatureList.sLensSendRawFileMediaTypes.isEnabled()
                        ? MimeTypeUtils.ALL_FILE_TYPES_MIME_TYPE
                        : MimeTypeUtils.PDF_MIME_TYPE;
        var i =
                new Intent(Intent.ACTION_OPEN_DOCUMENT)
                        .addCategory(Intent.CATEGORY_OPENABLE)
                        .setType(mimeType)
                        .putExtra(Intent.EXTRA_ALLOW_MULTIPLE, true)
                        .addFlags(
                                Intent.FLAG_GRANT_READ_URI_PERMISSION
                                        | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);

        mWindowAndroid.showCancelableIntent(
                i,
                (resultCode, data) -> {
                    if (resultCode != Activity.RESULT_OK || data == null) return;
                    if (!isInInputSession()) return;

                    try (var batchToken = mModelList.beginBatchEdit()) {
                        var uris = extractUrisFromResult(data);
                        for (var uri : uris) {
                            fetchAttachmentDetails(
                                    uri,
                                    this::uploadAndAddAttachment,
                                    FuseboxAttachmentButtonType.FILES);
                        }
                    }
                },
                /* errorId= */ android.R.string.cancel);
    }

    private void onClipboardClicked() {
        if (!isInInputSession()) return;

        mActionTaken = true;
        hidePopup();
        mMetrics.notifyAttachmentButtonUsed(FuseboxAttachmentButtonType.CLIPBOARD);
        if (isMaxAttachmentCountReached(FuseboxAttachmentType.ATTACHMENT_IMAGE)) return;

        long startTime = SystemClock.elapsedRealtime();
        new AsyncTask<byte[]>() {
            @Override
            protected byte[] doInBackground() {
                byte[] png = mClipboard.getPng();
                return png == null ? new byte[0] : png;
            }

            @Override
            protected void onPostExecute(byte[] pngBytes) {
                if (!isInInputSession()) return;
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
    /* package */ void fetchAttachmentDetails(
            Uri uri,
            Callback<@Nullable FuseboxAttachment> callback,
            @FuseboxAttachmentButtonType int buttonType) {
        new FuseboxAttachmentDetailsFetcher(
                        mContext, mContext.getContentResolver(), uri, callback, buttonType)
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Add an attachment to the Fusebox toolbar.
     *
     * @param attachment Contains information about the input that will be added as context.
     */
    /* package */ void uploadAndAddAttachment(@Nullable FuseboxAttachment attachment) {
        if (attachment == null) {
            onAttachmentUploadFailed();
            return;
        }
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

    private boolean isCurrentTab(Tab tab) {
        if (mTabModelSelectorSupplier == null || mTabModelSelectorSupplier.get() == null) {
            return false;
        }
        return mTabModelSelectorSupplier.get().getCurrentTab() == tab;
    }

    private void onInputStateChange(InputState inputState) {
        assert OmniboxFeatures.sShowModelPicker.getValue();
        // Note that some of the time that this method is called in the middle of beginInput(), so
        // checking avoid checking isInInputSession() or using mModelList.

        // TODO(https://crbug.com/480976526): Control visibility as well.
        boolean tabsEnabled =
                !inputState.disabledInputTypes.contains(InputType.INPUT_TYPE_BROWSER_TAB_VALUE);
        boolean imagesEnabled =
                !inputState.disabledInputTypes.contains(InputType.INPUT_TYPE_LENS_IMAGE_VALUE);
        boolean filesEnabled =
                !inputState.disabledInputTypes.contains(InputType.INPUT_TYPE_LENS_FILE_VALUE);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CURRENT_TAB_ENABLED, tabsEnabled);
        mModel.set(FuseboxProperties.POPUP_ATTACH_TAB_PICKER_ENABLED, tabsEnabled);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CLIPBOARD_ENABLED, imagesEnabled);
        mModel.set(FuseboxProperties.POPUP_ATTACH_CAMERA_ENABLED, imagesEnabled);
        mModel.set(FuseboxProperties.POPUP_ATTACH_GALLERY_ENABLED, imagesEnabled);
        mModel.set(FuseboxProperties.POPUP_ATTACH_FILE_ENABLED, filesEnabled);

        mModel.set(
                FuseboxProperties.POPUP_TOOL_HEADER_TEXT,
                inputState.toolsSectionConfig.getHeader());

        List<PopupButtonData> toolButtonDataList = new ArrayList<>();
        toolButtonDataList.add(
                new PopupButtonData(
                        this::onDynamicButtonClicked,
                        mContext.getString(R.string.ai_mode_entrypoint_label),
                        IconResourceIds.SEARCH_LOUPE_WITH_SPARKLE_VALUE,
                        /* enabled= */ true,
                        mInput != null
                                && mInput.getRequestType() == AutocompleteRequestType.AI_MODE,
                        PopupButtonType.TOOL,
                        ToolMode.TOOL_MODE_UNSPECIFIED_VALUE,
                        /* hasColor= */ false));

        for (ToolConfig toolConfig : inputState.toolConfigs) {
            int toolMode = toolConfig.getToolValue();
            if (!inputState.isToolVisible(toolMode)) continue;

            String label = toolConfig.getMenuLabel();
            int iconId =
                    toolConfig.hasIcon() && toolConfig.getIcon().hasIconId()
                            ? toolConfig.getIcon().getIconId().getNumber()
                            : IconResourceIds.PLACE_WHITE_VALUE;
            boolean selected =
                    mInput != null
                            && ToolModeUtils.getRequestTypeForToolMode(toolMode)
                                    == mInput.getRequestType();
            boolean enabled = inputState.isToolEnabled(toolMode);
            boolean hasColor =
                    toolMode == ToolMode.TOOL_MODE_IMAGE_GEN_VALUE
                            || toolMode == ToolMode.TOOL_MODE_IMAGE_GEN_UPLOAD_VALUE;

            toolButtonDataList.add(
                    new PopupButtonData(
                            this::onDynamicButtonClicked,
                            label,
                            iconId,
                            enabled,
                            selected,
                            PopupButtonType.TOOL,
                            toolMode,
                            hasColor));
        }

        mModel.set(FuseboxProperties.POPUP_TOOL_BUTTON_DATA_LIST, toolButtonDataList);

        // The InputState is always targeting an AI Mode request and what would be possible, but the
        // user might not have activate AI Mode yet, in which case we do not want to show any
        // selected model, even if we're going to automatically selected a default model as soon as
        // we transition to AI Mode.
        boolean isAimRequest =
                mInput != null && ToolModeUtils.isAimRequest(mInput.getRequestType());

        List<PopupButtonData> modelButtonDataList = new ArrayList<>();
        for (ModelConfig modelConfig : inputState.modelConfigs) {
            int modelMode = modelConfig.getModelValue();
            if (inputState.isModelVisible(modelMode)) {
                boolean selected = isAimRequest && inputState.activeModel == modelMode;
                int iconId =
                        modelConfig.hasIcon() && modelConfig.getIcon().hasIconId()
                                ? modelConfig.getIcon().getIconId().getNumber()
                                : IconResourceIds.PLACE_WHITE_VALUE;
                modelButtonDataList.add(
                        new PopupButtonData(
                                this::onDynamicButtonClicked,
                                modelConfig.getMenuLabel(),
                                iconId,
                                inputState.isModelEnabled(modelMode),
                                selected,
                                PopupButtonType.MODEL,
                                modelMode,
                                /* hasColor= */ false));
            }
        }
        boolean showModelPicker = modelButtonDataList.size() >= 2;
        boolean showModelPickerDivider =
                showModelPicker && !OmniboxFeatures.sShowBottomSheetPopup.getValue();
        mModel.set(FuseboxProperties.POPUP_MODEL_DIVIDER_VISIBLE, showModelPickerDivider);
        mModel.set(FuseboxProperties.POPUP_MODEL_HEADER_VISIBLE, showModelPicker);
        mModel.set(
                FuseboxProperties.POPUP_MODEL_HEADER_TEXT,
                inputState.modelSectionConfig.getHeader());
        mModel.set(
                FuseboxProperties.POPUP_MODEL_BUTTON_DATA_LIST,
                showModelPicker ? modelButtonDataList : List.of());
    }

    private boolean trySetRequestType(@AutocompleteRequestType int requestType) {
        if (!isInInputSession()) return false;

        hidePopup();
        if (mInput.getRequestType() == requestType) return false;

        mInput.setRequestType(requestType);
        return true;
    }

    private void onDynamicButtonClicked(PopupButtonData data) {
        mActionTaken = true;
        if (data.type == PopupButtonType.MODEL) {
            FuseboxMetrics.notifyModelButtonSelected(data.protoId);
            setModelMode(data.protoId);
        } else if (data.type == PopupButtonType.TOOL) {
            FuseboxMetrics.notifyToolButtonSelected(data.protoId);
            @AutocompleteRequestType
            int requestType = ToolModeUtils.getRequestTypeForToolMode(data.protoId);
            activateAiMode(requestType, AiModeActivationSource.TOOL_MENU);
        }
    }

    private void setModelMode(int modelMode) {
        assert OmniboxFeatures.sShowModelPicker.getValue();
        if (!isInInputSession()) return;

        hidePopup();

        maybeActivateAiMode(AiModeActivationSource.IMPLICIT);

        mInput.setModelMode(modelMode);
        // TODO(https://crbug.com/476434460): Consider replacing with wiring in session state.
        mComposeboxQueryControllerBridge.setActiveModel(modelMode);
    }

    private @FuseboxLayoutMode int getFuseboxLayoutMode() {
        return OmniboxFeatures.hasDesktopExperience(mContext)
                ? FuseboxLayoutMode.POPOVER
                : FuseboxLayoutMode.SEPARATED;
    }
}
