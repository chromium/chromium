// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.bottomsheet;

import android.content.Context;
import android.graphics.Bitmap;
import android.util.Pair;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.recyclerview.widget.RecyclerView;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.UnownedUserData;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.ContentPriority;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetControllerProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.webapps.AddToHomescreenProperties;
import org.chromium.components.webapps.AddToHomescreenViewDelegate;
import org.chromium.components.webapps.AppType;
import org.chromium.components.webapps.InstallTrigger;
import org.chromium.components.webapps.R;
import org.chromium.components.webapps.WebappInstallSource;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;

/** This class controls the Bottom Sheet PWA install functionality. */
@JNINamespace("webapps")
public class PwaBottomSheetController
        implements UnownedUserData, AddToHomescreenViewDelegate, View.OnClickListener {
    private final Context mContext;

    /** A pointer to the native version of this class. It's lifetime is controlled by this class. */
    private long mNativePwaBottomSheetController;

    /** The controller used to show the bottom sheet. */
    private BottomSheetController mBottomSheetController;

    /**
     * The observer used to set the bottom sheet content priority, communicate sheet state changes
     * to the native version of this class, and track when the sheet is dismissed.
     */
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetStateChanged(
                        @SheetState int state, @StateChangeReason int reason) {
                    if (state == SheetState.HIDDEN) {
                        if (reason == StateChangeReason.SWIPE) {
                            PwaBottomSheetControllerJni.get()
                                    .onSheetClosedWithSwipe(mNativePwaBottomSheetController);
                        }
                        mBottomSheetController.removeObserver(mBottomSheetObserver);
                        mWebContentsObserver = null;
                        mPwaBottomSheetContent = null;
                        destroy();
                        return;
                    }

                    // When our sheet is not fully expanded, lower its priority to make sure
                    // other (high-priority) sheets in the queue can be shown.
                    if (isBottomSheetVisible() && state == SheetState.FULL) {
                        mPwaBottomSheetContent.setPriority(ContentPriority.HIGH);
                        PwaBottomSheetControllerJni.get()
                                .onSheetExpanded(mNativePwaBottomSheetController);
                    } else {
                        mPwaBottomSheetContent.setPriority(ContentPriority.LOW);
                    }
                }
            };

    /** The Bottom Sheet content class for showing our content. */
    private PwaInstallBottomSheetContent mPwaBottomSheetContent;

    /** The property model for our bottom sheet. */
    private PropertyModel mModel;

    /** The adapter for handling the images inside the RecyclerView. */
    private ScreenshotsAdapter mScreenshotAdapter;

    /** The current WebContents the UI is associated with. */
    private WebContents mWebContents;

    /**
     * The observer to keep track of navigations (so the bottom sheet can close). May be null during
     * tests.
     */
    private WebContentsObserver mWebContentsObserver;

    /** The ViewHolder for the view's Screenshots RecyclerView. */
    private static class ScreenshotViewHolder extends RecyclerView.ViewHolder {
        public ScreenshotViewHolder(View itemView) {
            super(itemView);
        }
    }

    /** The Adapter for the view's Screenshots RecyclerView. */
    static class ScreenshotsAdapter extends RecyclerView.Adapter<ScreenshotViewHolder> {
        private Context mContext;
        private ArrayList<Bitmap> mScreenshots;

        public ScreenshotsAdapter(Context context) {
            mContext = context;
            mScreenshots = new ArrayList<Bitmap>();
        }

        @SuppressWarnings("NotifyDataSetChanged")
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
            Bitmap bitmap = mScreenshots.get(position);
            ImageView view = (ImageView) holder.itemView;
            view.setLayoutParams(
                    new ViewGroup.LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
            view.setAdjustViewBounds(true);
            view.setImageBitmap(bitmap);
            view.setContentDescription(
                    mContext.getResources()
                            .getString(R.string.pwa_install_bottom_sheet_screenshot));
            view.setOnClickListener(
                    v -> {
                        final ImageZoomView dialog = new ImageZoomView(mContext, bitmap);
                        dialog.show();
                    });
        }

        @Override
        public int getItemCount() {
            return mScreenshots != null ? mScreenshots.size() : 0;
        }
    }

    /**
     * Constructs a PwaBottomSheetController.
     *
     * @param context The current context.
     */
    public PwaBottomSheetController(Context context) {
        mContext = context;
    }

    // AddToHomescreenViewDelegate:

    @Override
    public void onAddToHomescreen(String title, @AppType int type) {
        onAddToHomescreen();
    }

    @Override
    public boolean onAppDetailsRequested() {
        return false;
    }

    @Override
    public void onViewDismissed() {
        // The bottom sheet observer OnSheetStateChanged() method is used instead to track when the
        // sheet is dismissed.
    }

    private void createWebContentsObserver(WebContents webContents) {
        assert mWebContentsObserver == null;
        mWebContentsObserver =
                new WebContentsObserver(webContents) {
                    @Override
                    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        if (navigation.hasCommitted()) {
                            mBottomSheetController.hideContent(
                                    mPwaBottomSheetContent, /* animate= */ true);
                        }
                    }
                };
    }

    /**
     * Makes a request to show the PWA Bottom Sheet Installer UI.
     *
     * @param nativePwaBottomSheetController The native controller to send requests to.
     * @param windowAndroid The window the UI is associated with.
     * @param webContents The WebContents the UI is associated with.
     * @param icon The icon of the app represented by the UI.
     * @param isAdaptiveIcon Whether the app icon is adaptive or not.
     * @param title The title of the app represented by the UI.
     * @param origin The origin of the PWA app.
     * @param description The app description.
     */
    public void requestBottomSheetInstaller(
            long nativePwaBottomSheetController,
            WindowAndroid windowAndroid,
            WebContents webContents,
            Bitmap icon,
            boolean isAdaptiveIcon,
            String title,
            String origin,
            String description) {
        assert mNativePwaBottomSheetController == 0;
        mNativePwaBottomSheetController = nativePwaBottomSheetController;
        mWebContents = webContents;

        mBottomSheetController = BottomSheetControllerProvider.from(windowAndroid);
        if (mBottomSheetController == null || !canShowFor(webContents)) {
            // TODO(finnur): Investigate whether retrying is feasible (and how).
            return;
        }

        mScreenshotAdapter = new ScreenshotsAdapter(mContext);
        PwaInstallBottomSheetView view =
                new PwaInstallBottomSheetView(mContext, mScreenshotAdapter);
        mPwaBottomSheetContent = new PwaInstallBottomSheetContent(view, this);
        mModel =
                new PropertyModel.Builder(AddToHomescreenProperties.ALL_KEYS)
                        .with(AddToHomescreenProperties.ICON, new Pair<>(icon, isAdaptiveIcon))
                        .with(AddToHomescreenProperties.TITLE, title)
                        .with(AddToHomescreenProperties.URL, origin)
                        .with(AddToHomescreenProperties.DESCRIPTION, description)
                        .with(AddToHomescreenProperties.CAN_SUBMIT, true)
                        .with(AddToHomescreenProperties.CLICK_LISTENER, this)
                        .build();
        PropertyModelChangeProcessor.create(
                mModel, view, AddToHomescreenBottomSheetViewBinder::bind);

        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (!mBottomSheetController.requestShowContent(mPwaBottomSheetContent, true)) {
            // TODO(finnur): Investigate whether retrying is feasible (and how).
            return;
        }

        if (webContents != null) {
            createWebContentsObserver(webContents);
        }
    }

    /**
     * @return Whether the Bottom Sheet Installer UI can be shown.
     * @param webContents The WebContents the UI should show for.
     */
    public boolean canShowFor(WebContents webContents) {
        return webContents.getVisibility() == Visibility.VISIBLE;
    }

    /** @return Whether the Bottom Sheet Installer UI sheet is visible. */
    public boolean isBottomSheetVisible() {
        return (mPwaBottomSheetContent != null
                && mBottomSheetController.getCurrentSheetContent() == mPwaBottomSheetContent);
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
     * Adds a screenshot to the currently showing UI.
     *
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
     * Makes a request to show the Bottom Sheet Installer UI in expanded state. If the UI is not
     * visible, it will be shown.
     *
     * @param webContents The WebContents the UI is associated with.
     * @param trigger The install trigger for the WebContents.
     * @return True if the bottom sheet is visible now, false otherwise.
     */
    public boolean requestOrExpandBottomSheetInstaller(
            WebContents webContents, @InstallTrigger int trigger) {
        return PwaBottomSheetControllerJni.get()
                .requestOrExpandBottomSheetInstaller(webContents, trigger);
    }

    /**
     * Makes a request to expand the Bottom Sheet Installer UI if visible already and notifies c++
     * side to track UI events.
     */
    public void expandBottomSheetInstaller() {
        if (!isBottomSheetVisible()) return;
        mBottomSheetController.expandSheet();
        PwaBottomSheetControllerJni.get().onSheetExpanded(mNativePwaBottomSheetController);
    }

    /**
     * Called when the source for webapp installation changes after controller was created.
     *
     * @param installSource The source for triggering webapp installation.
     */
    public void updateInstallSource(@WebappInstallSource int installSource) {
        PwaBottomSheetControllerJni.get()
                .updateInstallSource(mNativePwaBottomSheetController, installSource);
    }

    /** Called when the user wants to install. */
    public void onAddToHomescreen() {
        PwaBottomSheetControllerJni.get()
                .onAddToHomescreen(mNativePwaBottomSheetController, mWebContents);
    }

    /** Called when the install UI is dismissed to clean up the C++ side. */
    public void destroy() {
        PwaBottomSheetControllerJni.get().destroy(mNativePwaBottomSheetController);
        mNativePwaBottomSheetController = 0;
    }

    @NativeMethods
    interface Natives {
        boolean requestOrExpandBottomSheetInstaller(
                WebContents webContents, @InstallTrigger int trigger);

        void onSheetClosedWithSwipe(long nativePwaBottomSheetController);

        void onSheetExpanded(long nativePwaBottomSheetController);

        void updateInstallSource(
                long nativePwaBottomSheetController, @WebappInstallSource int installSource);

        void onAddToHomescreen(long nativePwaBottomSheetController, WebContents webContents);

        void destroy(long nativePwaBottomSheetController);
    }
}
