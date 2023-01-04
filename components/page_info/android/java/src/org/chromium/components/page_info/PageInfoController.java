// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.app.Activity;
import android.app.Dialog;
import android.content.ActivityNotFoundException;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.net.Uri;
import android.provider.Settings;
import android.text.Spannable;
import android.text.SpannableStringBuilder;
import android.text.style.TextAppearanceSpan;
import android.view.View;
import android.view.Window;
import android.widget.Button;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.core.view.ViewCompat;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.ContentSettingValues;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.omnibox.AutocompleteSchemeClassifier;
import org.chromium.components.omnibox.OmniboxUrlEmphasizer;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.BrowserContextHandle;
import org.chromium.content_public.browser.ContentFeatureList;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.ColorUtils;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.List;

/**
 * Java side of Android implementation of the page info UI.
 */
public class PageInfoController {
    @IntDef({OpenedFromSource.MENU, OpenedFromSource.TOOLBAR, OpenedFromSource.VR})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OpenedFromSource {
        int MENU = 1;
        int TOOLBAR = 2;
        int VR = 3;
    }

    @ContentSettingsType
    public static final int NO_HIGHLIGHTED_PERMISSION = ContentSettingsType.DEFAULT;

    private Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final WebContents mWebContents;

    // A pointer to the C++ object for this UI.
    private long mNativePageInfoController;

    // The security level of the page (a valid ConnectionSecurityLevel).
    private final @ConnectionSecurityLevel int mSecurityLevel;

    // Used to show Site settings from Page Info UI.
    private final PermissionParamsListBuilder mPermissionParamsListBuilder;


    private PageInfoListener mPageInfoListener;

    public interface PageInfoListener {

        void onAddPermissionSection(String name, String nameMidSentence, int type,
                                          @ContentSettingValues int currentSettingValue);

        void onUpdatePermissionDisplay();

        void onSetSecurityDescription(String summary, String details);

        void onUpdateTopicsDisplay(String[] topics);
    }

    public void setPageInfoListener(PageInfoListener listener) {
        mPageInfoListener = listener;
    }

    /**
     * Creates the PageInfoController, but does not display it. Also initializes the corresponding
     * C++ object and saves a pointer to it.
     * @param webContents              The WebContents showing the page that the PageInfo is about.
     */
    public PageInfoController(WebContents webContents, PageInfoListener listener) {
        mWebContents = webContents;
        mSecurityLevel = SecurityStateModel.getSecurityLevelForWebContents(webContents);
        mWindowAndroid = webContents.getTopLevelNativeWindow();
        mContext = mWindowAndroid.getContext().get();
        mPageInfoListener = listener;


        mPermissionParamsListBuilder = new PermissionParamsListBuilder(mContext, mWindowAndroid);
        mNativePageInfoController = PageInfoControllerJni.get().init(this, mWebContents);
    }

    public void destroy() {
        assert mNativePageInfoController != 0;
        PageInfoControllerJni.get().destroy(mNativePageInfoController, PageInfoController.this);
        mNativePageInfoController = 0;
        mContext = null;
    }

    public void clearData() {

    }

    /**
     * Adds a new row for the given permission.
     *
     * @param name The title of the permission to display to the user.
     * @param nameMidSentence The title of the permission to display to the user when used
     *         mid-sentence.
     * @param type The ContentSettingsType of the permission.
     * @param currentSettingValue The ContentSetting value of the currently selected setting.
     */
    @CalledByNative
    private void addPermissionSection(String name, String nameMidSentence, int type,
            @ContentSettingValues int currentSettingValue) {
        mPermissionParamsListBuilder.addPermissionEntry(
                name, nameMidSentence, type, currentSettingValue);
        if (mPageInfoListener != null) {
            mPageInfoListener.onAddPermissionSection(name, nameMidSentence, type, currentSettingValue);
        }
    }

    /**
     * Update the permissions view based on the contents of mDisplayedPermissions.
     */
    @CalledByNative
    private void updatePermissionDisplay() {
        if (mPageInfoListener != null) {
            mPageInfoListener.onUpdatePermissionDisplay();
        }
    }

    /**
     * Sets the connection security summary and detailed description strings. These strings may be
     * overridden based on the state of the Android UI.
     */
    @CalledByNative
    private void setSecurityDescription(String summary, String details) {
        if (mPageInfoListener != null) {
            mPageInfoListener.onSetSecurityDescription(summary, details);
        }
    }

    /**
     * Updates the Topic view if present.
     */
    @CalledByNative
    private void updateTopicsDisplay(String[] topics) {
        if (mPageInfoListener != null) {
            mPageInfoListener.onUpdateTopicsDisplay(topics);
        }
    }

    public void recordAction(@PageInfoAction int action) {
        assert mNativePageInfoController != 0;
        PageInfoControllerJni.get().recordPageInfoAction(
                mNativePageInfoController, PageInfoController.this, action);
    }

    public void setAboutThisSiteShown(boolean wasAboutThisSiteShown) {
        assert mNativePageInfoController != 0;
        PageInfoControllerJni.get().setAboutThisSiteShown(
                mNativePageInfoController, PageInfoController.this, wasAboutThisSiteShown);
    }

    public void refreshPermissions() {
        assert mNativePageInfoController != 0;
        mPermissionParamsListBuilder.clearPermissionEntries();
        PageInfoControllerJni.get().updatePermissions(
                mNativePageInfoController, PageInfoController.this);
    }

    public @ConnectionSecurityLevel int getSecurityLevel() {
        return mSecurityLevel;
    }

    @NativeMethods
    interface Natives {
        long init(PageInfoController controller, WebContents webContents);
        void destroy(long nativePageInfoControllerAndroid, PageInfoController caller);
        void recordPageInfoAction(
                long nativePageInfoControllerAndroid, PageInfoController caller, int action);
        void setAboutThisSiteShown(long nativePageInfoControllerAndroid, PageInfoController caller,
                boolean wasAboutThisSiteShown);
        void updatePermissions(long nativePageInfoControllerAndroid, PageInfoController caller);
    }

}
