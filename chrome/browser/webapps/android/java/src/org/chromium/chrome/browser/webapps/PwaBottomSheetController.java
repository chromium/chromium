// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.content.Context;
import android.graphics.Bitmap;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.UnownedUserData;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;

/**
 * This class controls the Bottom Sheet PWA install functionality.
 */
@JNINamespace("webapps")
public class PwaBottomSheetController
        implements UnownedUserData, AddToHomescreenViewDelegate, View.OnClickListener {
    private final Activity mActivity;

    /** A pointer to the native version of this class. It's lifetime is controlled by this class. */
    private long mNativePwaBottomSheetController;

    /** The controller used to show the bottom sheet. */
    private BottomSheetController mBottomSheetController;

    /** The Bottom Sheet content class for showing our content. */
    private PwaInstallBottomSheetContent mPwaBottomSheetContent;

    /** The property model for our bottom sheet. */
    private PropertyModel mModel;

    /** The adapter for handling the images inside the RecyclerView. */
    private ScreenshotsAdapter mScreenshotAdapter;

    /** The current WebContents the UI is associated with. */
    private WebContents mWebContents;

    /**
     * The observer to keep track of navigations (so the bottom sheet can
     * close). May be null during tests.
     */
    private WebContentsObserver mWebContentsObserver;

    /**
     * The ViewHolder for the view's Screenshots RecyclerView.
     */
    private class ScreenshotViewHolder extends RecyclerView.ViewHolder {
        public ScreenshotViewHolder(View itemView) {
            super(itemView);
        }
    }

    /**
     * The Adapter for the view's Screenshots RecyclerView.
     */
    class ScreenshotsAdapter extends RecyclerView.Adapter<ScreenshotViewHolder> {
        private Context mContext;
        private ArrayList<Bitmap> mScreenshots;

        public ScreenshotsAdapter(Context context) {
            mContext = context;
            mScreenshots = new ArrayList<Bitmap>();
        }

        public void addScreenshot(Bitmap screenshot) {
            mScreenshots.add(screenshot);
            notifyDataSetChanged();
        }

        @Override
        public ScreenshotViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            return new ScreenshotViewHolder(new ImageView(mContext));
        }

        @Override
        public void onBindViewHolder(ScreenshotViewHolder holder, int position) {
            ImageView view = (ImageView) holder.itemView;
            view.setImageBitmap(mScreenshots.get(position));
        }

        @Override
        public int getItemCount() {
            return mScreenshots != null ? mScreenshots.size() : 0;
        }
    }

    /**
     * Constructs a PwaBottomSheetController.
     * @param activity The current activity.
     */
    public PwaBottomSheetController(Activity activity) {
        mActivity = activity;
    }

    // AddToHomescreenViewDelegate:

    @Override
    public void onAddToHomescreen(String title) {
        onAddToHomescreen();
    }

    @Override
    public boolean onAppDetailsRequested() {
        return false;
    }

    @Override
    public void onViewDismissed() {
        mWebContentsObserver = null;
        mPwaBottomSheetContent = null;
        destroy();
    }

    private void createWebContentsObserver(WebContents webContents) {
        assert mWebContentsObserver == null;
        mWebContentsObserver = new WebContentsObserver(webContents) {
            @Override
            public void didFinishNavigation(NavigationHandle navigation) {
                if (navigation.isInMainFrame() && navigation.hasCommitted()) {
                    mBottomSheetController.hideContent(mPwaBottomSheetContent, /* animate= */ true);
                }
            }
        };
    }

    /**
     * Makes a request to show the PWA Bottom Sheet Installer UI.
     * @param nativePwaBottomSheetController The native controller to send
     *         requests to.
     * @param windowAndroid The window the UI is associated with.
     * @param webContents The WebContents the UI is associated with.
     * @param showExpanded Whether to show the UI in expanded mode or not.
     * @param icon The icon of the app represented by the UI.
     * @param isAdaptiveIcon Whether the app icon is adaptive or not.
     * @param title The title of the app represented by the UI.
     * @param origin The origin of the PWA app.
     * @param description The app description.
     * @param categories The categories this app falls under.
     */
    public void requestBottomSheetInstaller(long nativePwaBottomSheetController,
            WindowAndroid windowAndroid, WebContents webContents, boolean showExpanded, Bitmap icon,
            boolean isAdaptiveIcon, String title, String origin, String description,
            String categories) {
        mNativePwaBottomSheetController = nativePwaBottomSheetController;
        mWebContents = webContents;

        mBottomSheetController = BottomSheetControllerProvider.from(windowAndroid);
        if (mBottomSheetController == null) {
            // TODO(finnur): Investigate whether retrying is feasible (and how).
            return;
        }

        mScreenshotAdapter = new ScreenshotsAdapter(mActivity);
        PwaInstallBottomSheetView view =
                new PwaInstallBottomSheetView(mActivity, mScreenshotAdapter);
        mPwaBottomSheetContent = new PwaInstallBottomSheetContent(view, this);
        mModel = new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS)
                         .with(AddToHomescreenProperties.ICON, new Pair<>(icon, isAdaptiveIcon))
                         .with(AddToHomescreenProperties.TITLE, title)
                         .with(AddToHomescreenProperties.URL, origin)
                         .with(AddToHomescreenProperties.DESCRIPTION, description)
                         .with(AddToHomescreenProperties.CATEGORIES, categories)
                         .with(AddToHomescreenProperties.CAN_SUBMIT, true)
                         .with(AddToHomescreenProperties.CLICK_LISTENER, this)
                         .build();
        PropertyModelChangeProcessor.create(
                mModel, view, AddToHomescreenBottomSheetViewBinder::bind);

        if (!mBottomSheetController.requestShowContent(mPwaBottomSheetContent, true)) {
            // TODO(finnur): Investigate whether retrying is feasible (and how).
            return;
        }

        if (showExpanded) {
            mBottomSheetController.expandSheet();
        }

        if (webContents != null) {
            createWebContentsObserver(webContents);
        }
    }

    // onClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.button_install) {
            onAddToHomescreen();
            mBottomSheetController.hideContent(mPwaBottomSheetContent, false);
        } else if (id == R.id.drag_handlebar) {
            if (mBottomSheetController.isSheetOpen()) {
                mBottomSheetController.collapseSheet(true);
            } else {
                mBottomSheetController.expandSheet();
            }
        }
    }

    /**
     * Makes a request to show the Bottom Sheet Installer UI in expanded state. If the
     * UI is not visible, it will be shown.
     * @param webContents The WebContents the UI is associated with.
     */
    public void requestOrExpandBottomSheetInstaller(WebContents webContents) {
        if (mPwaBottomSheetContent != null
                && mBottomSheetController.getCurrentSheetContent() == mPwaBottomSheetContent) {
            mBottomSheetController.expandSheet();
            return;
        }

        PwaBottomSheetControllerJni.get().createAndShowBottomSheetInstaller(webContents);
    }

    /**
     * Adds a screenshot to the currently showing UI.
     * @param screenshot The screenshot to add to the list of screenshots.
     * @param webContents The WebContents the UI is associated with.
     */
    @CalledByNative
    private static void addWebAppScreenshot(Bitmap screenshot, WebContents webContents) {
        WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return;
        PwaBottomSheetController controller = PwaBottomSheetControllerProvider.from(window);
        if (controller == null) return;
        controller.addWebAppScreenshot(screenshot);
    }

    private void addWebAppScreenshot(Bitmap screenshot) {
        mScreenshotAdapter.addScreenshot(screenshot);
    }

    // JNI wrapper methods:

    /**
     * Called when the user wants to install.
     */
    public void onAddToHomescreen() {
        PwaBottomSheetControllerJni.get().onAddToHomescreen(
                mNativePwaBottomSheetController, mWebContents);
    }

    /**
     * Called when the install UI is dismissed to clean up the C++ side.
     */
    public void destroy() {
        PwaBottomSheetControllerJni.get().destroy(mNativePwaBottomSheetController);
    }

    @NativeMethods
    interface Natives {
        void createAndShowBottomSheetInstaller(WebContents webContents);
        void onAddToHomescreen(long nativePwaBottomSheetController, WebContents webContents);
        void destroy(long nativePwaBottomSheetController);
    }
}
