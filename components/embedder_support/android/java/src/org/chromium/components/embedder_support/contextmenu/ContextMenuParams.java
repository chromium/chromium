// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.embedder_support.contextmenu;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

import org.chromium.blink_public.common.ContextMenuDataMediaFlags;
import org.chromium.blink_public.common.ContextMenuDataMediaType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.AdditionalNavigationParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.listmenu.MenuModelBridge;
import org.chromium.url.GURL;

/**
 * A list of parameters that explain what kind of context menu to show the user. This data is
 * generated from components/embedder_support/android/contextmenu/context_menu_builder.h.
 */
@JNINamespace("context_menu")
@NullMarked
public class ContextMenuParams {
    private final long mNativePtr;
    private final GURL mPageUrl;
    private final GURL mLinkUrl;
    private final String mLinkText;
    private final String mTitleText;
    private final GURL mUnfilteredLinkUrl;
    private final GURL mSrcUrl;
    private final @Nullable Referrer mReferrer;

    private final boolean mIsPage;
    private final boolean mIsAnchor;
    private final boolean mIsImage;
    private final boolean mIsVideo;
    private final boolean mCanSaveMedia;

    private final @ContextMenuDataMediaFlags int mMediaFlags;

    private final int mTriggeringTouchXDp;
    private final int mTriggeringTouchYDp;

    private final int mSourceType;

    private final boolean mOpenedFromHighlight;

    private final boolean mOpenedFromInterestFor;
    private final int mInterestForNodeID;

    private final @Nullable AdditionalNavigationParams mAdditionalNavigationParams;
    private final MenuModelBridge mMenuModelBridge;

    @CalledByNative
    private long getNativePointer() {
        return mNativePtr;
    }

    /** @return The URL associated with the main frame of the page that triggered the context menu. */
    public GURL getPageUrl() {
        return mPageUrl;
    }

    /** @return The link URL, if any. */
    public GURL getLinkUrl() {
        return mLinkUrl;
    }

    /** @return The link text, if any. */
    public String getLinkText() {
        return mLinkText;
    }

    /** @return The title or alt attribute (if title is not available). */
    public String getTitleText() {
        return mTitleText;
    }

    /** @return The unfiltered link URL, if any. */
    public GURL getUnfilteredLinkUrl() {
        return mUnfilteredLinkUrl;
    }

    /** @return The source URL. */
    public GURL getSrcUrl() {
        return mSrcUrl;
    }

    /** @return the referrer associated with the frame on which the menu is invoked */
    public @Nullable Referrer getReferrer() {
        return mReferrer;
    }

    /**
     * @return Whether or not the context menu is being shown for a page.
     */
    public boolean isPage() {
        return mIsPage;
    }

    /**
     * @return Whether or not the context menu is being shown for an anchor.
     */
    public boolean isAnchor() {
        return mIsAnchor;
    }

    /** @return Whether or not the context menu is being shown for an image. */
    public boolean isImage() {
        return mIsImage;
    }

    /** @return Whether or not the context menu is being shown for a video. */
    public boolean isVideo() {
        return mIsVideo;
    }

    public boolean canSaveMedia() {
        return mCanSaveMedia;
    }

    /**
     * @return Whether the media element is eligible to enter Picture-in-Picture.
     */
    public boolean canPictureInPicture() {
        return (mMediaFlags & ContextMenuDataMediaFlags.MEDIA_CAN_PICTURE_IN_PICTURE) != 0;
    }

    /**
     * @return Whether the media element is currently in Picture-in-Picture.
     */
    public boolean isPictureInPicture() {
        return (mMediaFlags & ContextMenuDataMediaFlags.MEDIA_PICTURE_IN_PICTURE) != 0;
    }

    /**
     * @return The x-coordinate of the touch that triggered the context menu in dp relative to the
     *     render view; 0 corresponds to the left edge.
     */
    public int getTriggeringTouchXDp() {
        return mTriggeringTouchXDp;
    }

    /**
     * @return The y-coordinate of the touch that triggered the context menu in dp relative to the
     *         render view; 0 corresponds to the left edge.
     */
    public int getTriggeringTouchYDp() {
        return mTriggeringTouchYDp;
    }

    /**
     * @return The method used to cause the context menu to be shown. For example, right mouse click
     *         or long press.
     */
    public int getSourceType() {
        return mSourceType;
    }

    /** @return Whether or not the context menu is been shown for a download item. */
    public boolean isFile() {
        if (getSrcUrl().getScheme().equals(ContentUrlConstants.FILE_SCHEME)) {
            return true;
        }
        return false;
    }

    /** @return The valid url of a ContextMenuParams. */
    public GURL getUrl() {
        if (isAnchor() && !getLinkUrl().isEmpty()) {
            return getLinkUrl();
        } else {
            return getSrcUrl();
        }
    }

    /** @return Whether or not the context menu was opened from highlight. */
    public boolean getOpenedFromHighlight() {
        return mOpenedFromHighlight;
    }

    /**
     * @return Whether or not the context menu was opened from an element with the `interestfor`
     *     attribute.
     */
    public boolean getOpenedFromInterestFor() {
        return mOpenedFromInterestFor;
    }

    /**
     * @return Only valid if `getOpenedFromInterestFor()` is true. This returns the DOMNodeID
     *     for the element that should be "shown interest" in case the "show interest" menu
     *     item is chosen by the user.
     */
    public int getInterestForNodeID() {
        return mInterestForNodeID;
    }

    /**
     * @return The additional navigation params associated with this Context Menu.
     */
    public @Nullable AdditionalNavigationParams getAdditionalNavigationParams() {
        return mAdditionalNavigationParams;
    }

    /** Returns the {@link MenuModelBridge} associated with this context menu. */
    public MenuModelBridge getMenuModelBridge() {
        return mMenuModelBridge;
    }

    @VisibleForTesting
    public ContextMenuParams(
            long nativePtr,
            MenuModelBridge menuModelBridge,
            @ContextMenuDataMediaType int mediaType,
            @ContextMenuDataMediaFlags int mediaFlags,
            GURL pageUrl,
            GURL linkUrl,
            String linkText,
            GURL unfilteredLinkUrl,
            GURL srcUrl,
            String titleText,
            @Nullable Referrer referrer,
            boolean canSaveMedia,
            int triggeringTouchXDp,
            int triggeringTouchYDp,
            int sourceType,
            boolean openedFromHighlight,
            boolean openedFromInterestFor,
            int interestForNodeID,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        mNativePtr = nativePtr;
        mMenuModelBridge = menuModelBridge;
        mPageUrl = pageUrl;
        mLinkUrl = linkUrl;
        mLinkText = linkText;
        mTitleText = titleText;
        mUnfilteredLinkUrl = unfilteredLinkUrl;
        mSrcUrl = srcUrl;
        mReferrer = referrer;

        // Note: On desktop it is necessary to also check for the case where the target is an
        // (editable) text/ password selection. Here that is not necessary because on Clank
        //  it will open a selection popup instead of a context menu.
        mIsPage =
                (mediaType == ContextMenuDataMediaType.NONE
                        && linkUrl.isEmpty()
                        && !openedFromHighlight);
        mIsAnchor = !linkUrl.isEmpty();
        mIsImage = mediaType == ContextMenuDataMediaType.IMAGE;
        mIsVideo = mediaType == ContextMenuDataMediaType.VIDEO;
        mCanSaveMedia = canSaveMedia;
        mMediaFlags = mediaFlags;
        mTriggeringTouchXDp = triggeringTouchXDp;
        mTriggeringTouchYDp = triggeringTouchYDp;
        mSourceType = sourceType;
        mOpenedFromHighlight = openedFromHighlight;
        mOpenedFromInterestFor = openedFromInterestFor;
        mInterestForNodeID = interestForNodeID;
        mAdditionalNavigationParams = additionalNavigationParams;
    }

    @CalledByNative
    private static ContextMenuParams create(
            long nativePtr,
            MenuModelBridge menuModelBridge,
            @ContextMenuDataMediaType int mediaType,
            @ContextMenuDataMediaFlags int mediaFlags,
            GURL pageUrl,
            GURL linkUrl,
            String linkText,
            GURL unfilteredLinkUrl,
            GURL srcUrl,
            String titleText,
            GURL sanitizedReferrer,
            int referrerPolicy,
            boolean canSaveMedia,
            int triggeringTouchXDp,
            int triggeringTouchYDp,
            int sourceType,
            boolean openedFromHighlight,
            boolean openedFromInterestFor,
            int interestForNodeID,
            @Nullable AdditionalNavigationParams additionalNavigationParams) {
        // TODO(crbug.com/40549331): Convert Referrer to use GURL.
        Referrer referrer =
                sanitizedReferrer.isEmpty()
                        ? null
                        : new Referrer(sanitizedReferrer.getSpec(), referrerPolicy);
        return new ContextMenuParams(
                nativePtr,
                menuModelBridge,
                mediaType,
                mediaFlags,
                pageUrl,
                linkUrl,
                linkText,
                unfilteredLinkUrl,
                srcUrl,
                titleText,
                referrer,
                canSaveMedia,
                triggeringTouchXDp,
                triggeringTouchYDp,
                sourceType,
                openedFromHighlight,
                openedFromInterestFor,
                interestForNodeID,
                additionalNavigationParams);
    }
}
