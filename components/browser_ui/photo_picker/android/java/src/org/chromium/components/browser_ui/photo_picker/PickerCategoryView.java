// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.content.ContentResolver;
import android.content.Context;
import android.content.DialogInterface;
import android.content.res.Configuration;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.net.Uri;
import android.os.SystemClock;
import android.transition.ChangeBounds;
import android.transition.Transition;
import android.transition.TransitionManager;
import android.util.DisplayMetrics;
import android.util.LruCache;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.RelativeLayout;

import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.DiscardableReferencePool.DiscardableReference;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.components.browser_ui.util.ConversionUtils;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.browser_ui.widget.selectable_list.SelectableListLayout;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.net.MimeTypeFilter;
import org.chromium.ui.base.PhotoPickerListener;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;

/**
 * A class for keeping track of common data associated with showing photos in the photo picker, for
 * example the RecyclerView and the bitmap caches.
 */
public class PickerCategoryView extends RelativeLayout
        implements FileEnumWorkerTask.FilesEnumeratedCallback,
                RecyclerView.RecyclerListener,
                DecoderServiceHost.DecoderStatusCallback,
                View.OnClickListener,
                SelectionDelegate.SelectionObserver<PickerBitmap> {
    // These values are written to logs.  New enum values can be added, but existing
    // enums must never be renumbered or deleted and reused.
    private static final int ACTION_CANCEL = 0;
    private static final int ACTION_PHOTO_PICKED = 1;
    private static final int ACTION_NEW_PHOTO = 2;
    private static final int ACTION_BROWSE = 3;
    private static final int ACTION_BOUNDARY = 4;

    /**
     * A container class for keeping track of the data we need to show a photo/video tile in the
     * photo picker (the data we store in the cache).
     */
    public static class Thumbnail {
        public List<Bitmap> bitmaps;
        public Boolean fullWidth;
        public String videoDuration;

        // The calculated ratio of the originals for the bitmaps above, were they to be shown
        // un-cropped. NOTE: The |bitmaps| above may already have been cropped and as such might
        // have a different ratio.
        public float ratioOriginal;

        Thumbnail(List<Bitmap> bitmaps, String videoDuration, Boolean fullWidth, float ratio) {
            this.bitmaps = bitmaps;
            this.videoDuration = videoDuration;
            this.fullWidth = fullWidth;
            this.ratioOriginal = ratio;
        }
    }

    // The dialog that owns us.
    private PhotoPickerDialog mDialog;

    // The view containing the RecyclerView and the toolbar, etc.
    private SelectableListLayout<PickerBitmap> mSelectableListLayout;

    // The {@link WindowAndroid} for the hosting WebContents.
    private WindowAndroid mWindowAndroid;

    // The ContentResolver to use to retrieve image metadata from disk.
    private ContentResolver mContentResolver;

    // The list of images on disk, sorted by last-modified first.
    private List<PickerBitmap> mPickerBitmaps;

    // True if multi-selection is allowed in the picker.
    private boolean mMultiSelectionAllowed;

    // The callback to notify the listener of decisions reached in the picker.
    private PhotoPickerListener mListener;

    // The host class for the decoding service.
    private DecoderServiceHost mDecoderServiceHost;

    // The RecyclerView showing the images.
    private RecyclerView mRecyclerView;

    // The {@link PickerAdapter} for the RecyclerView.
    private PickerAdapter mPickerAdapter;

    // The layout manager for the RecyclerView.
    private GridLayoutManager mLayoutManager;

    // The decoration to use for the RecyclerView.
    private GridSpacingItemDecoration mSpacingDecoration;

    // The {@link SelectionDelegate} keeping track of which images are selected.
    private SelectionDelegate<PickerBitmap> mSelectionDelegate;

    // A low-resolution cache for thumbnails, lazily created. Helpful for cache misses from the
    // high-resolution cache to avoid showing gray squares (we show pixelated versions instead until
    // image can be loaded off disk, which is much less jarring).
    private DiscardableReference<LruCache<String, Thumbnail>> mLowResThumbnails;

    // A high-resolution cache for thumbnails, lazily created.
    private DiscardableReference<LruCache<String, Thumbnail>> mHighResThumbnails;

    // A cache for full-screen versions of images, lazily created.
    private DiscardableReference<LruCache<String, Thumbnail>> mFullScreenBitmaps;

    // The size of the low-res cache.
    private int mCacheSizeLarge;

    // The size of the high-res cache.
    private int mCacheSizeSmall;

    // The size of the full-screen cache.
    private int mCacheSizeFullScreen;

    // Whether we are in magnifying mode (one image per column).
    private boolean mMagnifyingMode;

    // Whether we are in the middle of animating between magnifying modes.
    private boolean mZoomSwitchingInEffect;

    /**
     * The number of columns to show. Note: mColumns and mPadding (see below) should both be even
     * numbers or both odd, not a mix (the column padding will not be of uniform thickness if they
     * are a mix).
     */
    private int mColumns;

    // The padding between columns. See also comment for mColumns.
    private int mPadding;

    // The width of the bitmaps.
    private int mImageWidth;

    // The height of the special tiles.
    private int mSpecialTileHeight;

    // A worker task for asynchronously enumerating files off the main thread.
    private FileEnumWorkerTask mWorkerTask;

    // The timestamp for the start of the enumeration of files on disk.
    private long mEnumStartTime;

    // Whether the connection to the service has been established.
    private boolean mServiceReady;

    // The MIME types requested.
    private List<String> mMimeTypes;

    // A list of files to use for testing (instead of reading files on disk).
    private static List<PickerBitmap> sTestFiles;

    // The Video Player.
    private final PickerVideoPlayer mVideoPlayer;

    // The Zoom (floating action) button.
    private ImageView mZoom;

    /**
     * @param windowAndroid The window of the {@link WebContents} that requested the photo
     *     selection.
     * @param contentResolver The ContentResolver to use to retrieve image metadata from disk.
     * @param multiSelectionAllowed Whether to allow the user to select more than one image.
     */
    @SuppressWarnings("unchecked") // mSelectableListLayout
    public PickerCategoryView(
            WindowAndroid windowAndroid,
            ContentResolver contentResolver,
            boolean multiSelectionAllowed,
            PhotoPickerToolbar.PhotoPickerToolbarDelegate delegate) {
        super(windowAndroid.getContext().get());
        mWindowAndroid = windowAndroid;
        Context context = mWindowAndroid.getContext().get();
        mContentResolver = contentResolver;
        mMultiSelectionAllowed = multiSelectionAllowed;

        mDecoderServiceHost = new DecoderServiceHost(this, context);
        mDecoderServiceHost.bind();

        mSelectionDelegate = new SelectionDelegate<PickerBitmap>();
        mSelectionDelegate.addObserver(this);
        if (!multiSelectionAllowed) mSelectionDelegate.setSingleSelectionMode();

        View root = LayoutInflater.from(context).inflate(R.layout.photo_picker_dialog, this);
        mSelectableListLayout =
                (SelectableListLayout<PickerBitmap>) root.findViewById(R.id.selectable_list);

        mPickerAdapter = new PickerAdapter(this);
        mRecyclerView = mSelectableListLayout.initializeRecyclerView(mPickerAdapter);
        int titleId =
                multiSelectionAllowed
                        ? R.string.photo_picker_select_images
                        : R.string.photo_picker_select_image;
        PhotoPickerToolbar toolbar =
                (PhotoPickerToolbar)
                        mSelectableListLayout.initializeToolbar(
                                R.layout.photo_picker_toolbar,
                                mSelectionDelegate,
                                titleId,
                                0,
                                0,
                                null,
                                false);
        toolbar.setNavigationOnClickListener(this);
        toolbar.setDelegate(delegate);
        Button doneButton = (Button) toolbar.findViewById(R.id.done);
        doneButton.setOnClickListener(this);
        mVideoPlayer = findViewById(R.id.playback_container);
        mZoom = findViewById(R.id.zoom);

        calculateGridMetrics();

        mLayoutManager = new GridLayoutManager(context, mColumns);
        mRecyclerView.setHasFixedSize(true);
        mRecyclerView.setLayoutManager(mLayoutManager);
        mSpacingDecoration = new GridSpacingItemDecoration(mColumns, mPadding);
        mRecyclerView.addItemDecoration(mSpacingDecoration);
        mRecyclerView.setRecyclerListener(this);

        final long maxMemory = ConversionUtils.bytesToKilobytes(Runtime.getRuntime().maxMemory());
        mCacheSizeFullScreen = (int) (maxMemory / 4); // 1/4 of the available memory.
        mCacheSizeLarge = (int) (maxMemory / 4); // 1/4 of the available memory.
        mCacheSizeSmall = (int) (maxMemory / 8); // 1/8th of the available memory.
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        calculateGridMetrics();
        mLayoutManager.setSpanCount(mColumns);
        mRecyclerView.removeItemDecoration(mSpacingDecoration);
        mSpacingDecoration = new GridSpacingItemDecoration(mColumns, mPadding);
        mRecyclerView.addItemDecoration(mSpacingDecoration);

        // Configuration change can happen at any time, even before the photos have been
        // enumerated (when mPickerBitmaps is null, causing: https://crbug.com/947657). There's no
        // need to call notifyDataSetChanged in that case because it will be called once the photo
        // list becomes ready.
        if (mPickerBitmaps != null) {
            mPickerAdapter.notifyDataSetChanged();
            ViewUtils.requestLayout(mRecyclerView, "PickerCategoryView.onConfigurationChanged");
        }
    }

    /** Severs the connection to the decoding utility process and cancels any outstanding requests. */
    public void onDialogDismissed() {
        if (mWorkerTask != null) {
            mWorkerTask.cancel(true);
            mWorkerTask = null;
        }

        if (mDecoderServiceHost != null) {
            mDecoderServiceHost.unbind();
            mDecoderServiceHost = null;
        }

        mDialog = null;
    }

    /**
     * Start playback of a video in an overlay above the photo picker.
     *
     * @param uri The uri of the video to start playing.
     */
    public void startVideoPlaybackAsync(Uri uri) {
        if (mDialog == null) return;
        mVideoPlayer.startVideoPlaybackAsync(uri, mDialog.getWindow());
    }

    /**
     * Ends video playback (if a video is playing) and closes the video player. Aborts if the video
     * playback container is not showing.
     *
     * @return true if a video container was showing, false otherwise.
     */
    public boolean closeVideoPlayer() {
        return mVideoPlayer.closeVideoPlayer();
    }

    /**
     * Initializes the PickerCategoryView object.
     *
     * @param dialog The dialog showing us.
     * @param listener The listener who should be notified of actions.
     * @param mimeTypes A list of mime types to show in the dialog.
     */
    public void initialize(
            PhotoPickerDialog dialog, PhotoPickerListener listener, List<String> mimeTypes) {
        mDialog = dialog;
        mListener = listener;
        mMimeTypes = new ArrayList<>(mimeTypes);

        enumerateBitmaps();

        mDialog.setOnCancelListener(
                new DialogInterface.OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        executeAction(
                                PhotoPickerListener.PhotoPickerAction.CANCEL, null, ACTION_CANCEL);
                    }
                });
    }

    // FileEnumWorkerTask.FilesEnumeratedCallback:

    @Override
    public void filesEnumeratedCallback(List<PickerBitmap> files) {
        if (files == null) {
            return;
        }

        // Calculate the rate of files enumerated per tenth of a second.
        long elapsedTimeMs = SystemClock.elapsedRealtime() - mEnumStartTime;
        int rate = (int) (100L * files.size() / elapsedTimeMs);
        RecordHistogram.recordTimesHistogram("Android.PhotoPicker.EnumerationTime", elapsedTimeMs);
        RecordHistogram.recordCustomCountHistogram(
                "Android.PhotoPicker.EnumeratedFiles", files.size(), 1, 10000, 50);
        RecordHistogram.recordCount1000Histogram("Android.PhotoPicker.EnumeratedRate", rate);

        mPickerBitmaps = files;
        processBitmaps();
    }

    // DecoderServiceHost.DecoderStatusCallback:

    @Override
    public void serviceReady() {
        mServiceReady = true;
        processBitmaps();
    }

    @Override
    public void decoderIdle() {}

    // RecyclerView.RecyclerListener:

    @Override
    public void onViewRecycled(RecyclerView.ViewHolder holder) {
        PickerBitmapViewHolder bitmapHolder = (PickerBitmapViewHolder) holder;
        String filePath = bitmapHolder.getFilePath();
        if (filePath != null) {
            getDecoderServiceHost().cancelDecodeImage(filePath);
        }
    }

    // SelectionDelegate.SelectionObserver:

    @Override
    public void onSelectionStateChange(List<PickerBitmap> selectedItems) {
        if (mZoom.getVisibility() != View.VISIBLE) {
            mZoom.setVisibility(View.VISIBLE);
            mZoom.setOnClickListener(this);
        }
    }

    // OnClickListener:

    @Override
    public void onClick(View view) {
        int id = view.getId();
        if (id == R.id.done) {
            notifyPhotosSelected();
        } else if (id == R.id.zoom) {
            if (!mZoomSwitchingInEffect) {
                flipZoomMode();
            }
        } else {
            executeAction(PhotoPickerListener.PhotoPickerAction.CANCEL, null, ACTION_CANCEL);
        }
    }

    /** Start loading of bitmaps, once files have been enumerated and service is ready to decode. */
    private void processBitmaps() {
        if (mServiceReady && mPickerBitmaps != null) {
            mPickerAdapter.notifyDataSetChanged();
        }
    }

    private void flipZoomMode() {
        // Bitmap scaling is cumulative, so if an image is selected when we switch modes, it will
        // become skewed when switching between full size and square modes because dimensions of the
        // picture also change (from square to full width). We therefore un-select all items before
        // starting the animation and then reselect them once animation has ended.
        final HashSet<PickerBitmap> selectedItems =
                new HashSet<>(mSelectionDelegate.getSelectedItems());
        mSelectionDelegate.clearSelection();

        mMagnifyingMode = !mMagnifyingMode;

        Context context = mWindowAndroid.getContext().get();
        if (mMagnifyingMode) {
            mZoom.setImageResource(R.drawable.zoom_out);
            mZoom.setContentDescription(
                    context.getString(R.string.photo_picker_accessibility_zoom_out));
        } else {
            mZoom.setImageResource(R.drawable.zoom_in);
            mZoom.setContentDescription(
                    context.getString(R.string.photo_picker_accessibility_zoom_in));
        }

        calculateGridMetrics();

        if (!mMagnifyingMode) {
            getFullScreenBitmaps().evictAll();
        }

        mZoomSwitchingInEffect = true;

        ChangeBounds transition = new ChangeBounds();
        transition.addListener(
                new Transition.TransitionListener() {
                    @Override
                    public void onTransitionStart(Transition transition) {}

                    @Override
                    public void onTransitionEnd(Transition transition) {
                        mZoomSwitchingInEffect = false;

                        // Redo selection when switching between modes to make it obvious what got
                        // selected.
                        mSelectionDelegate.setSelectedItems(selectedItems);
                    }

                    @Override
                    public void onTransitionCancel(Transition transition) {}

                    @Override
                    public void onTransitionPause(Transition transition) {}

                    @Override
                    public void onTransitionResume(Transition transition) {}
                });

        TransitionManager.beginDelayedTransition(mRecyclerView, transition);

        mLayoutManager.setSpanCount(mColumns);
        mPickerAdapter.notifyDataSetChanged();
        ViewUtils.requestLayout(mRecyclerView, "PickerCategoryView.flipZoomMode");
    }

    // Simple accessors:

    public int getImageWidth() {
        return mImageWidth;
    }

    public int getSpecialTileHeight() {
        return mSpecialTileHeight;
    }

    public boolean isInMagnifyingMode() {
        return mMagnifyingMode;
    }

    public boolean isZoomSwitchingInEffect() {
        return mZoomSwitchingInEffect;
    }

    public SelectionDelegate<PickerBitmap> getSelectionDelegate() {
        return mSelectionDelegate;
    }

    public List<PickerBitmap> getPickerBitmaps() {
        return mPickerBitmaps;
    }

    public DecoderServiceHost getDecoderServiceHost() {
        return mDecoderServiceHost;
    }

    public LruCache<String, Thumbnail> getLowResThumbnails() {
        if (mLowResThumbnails == null || mLowResThumbnails.get() == null) {
            mLowResThumbnails =
                    GlobalDiscardableReferencePool.getReferencePool()
                            .put(new LruCache<String, Thumbnail>(mCacheSizeSmall));
        }
        return mLowResThumbnails.get();
    }

    public LruCache<String, Thumbnail> getHighResThumbnails() {
        if (mHighResThumbnails == null || mHighResThumbnails.get() == null) {
            mHighResThumbnails =
                    GlobalDiscardableReferencePool.getReferencePool()
                            .put(new LruCache<String, Thumbnail>(mCacheSizeLarge));
        }
        return mHighResThumbnails.get();
    }

    public LruCache<String, Thumbnail> getFullScreenBitmaps() {
        if (mFullScreenBitmaps == null || mFullScreenBitmaps.get() == null) {
            mFullScreenBitmaps =
                    GlobalDiscardableReferencePool.getReferencePool()
                            .put(new LruCache<String, Thumbnail>(mCacheSizeFullScreen));
        }
        return mFullScreenBitmaps.get();
    }

    public boolean isMultiSelectAllowed() {
        return mMultiSelectionAllowed;
    }

    /** Notifies the listener that the user selected to launch the gallery. */
    public void showGallery() {
        executeAction(PhotoPickerListener.PhotoPickerAction.LAUNCH_GALLERY, null, ACTION_BROWSE);
    }

    /** Notifies the listener that the user selected to launch the camera intent. */
    public void showCamera() {
        executeAction(PhotoPickerListener.PhotoPickerAction.LAUNCH_CAMERA, null, ACTION_NEW_PHOTO);
    }

    /** Calculates image size and how many columns can fit on-screen. */
    private void calculateGridMetrics() {
        DisplayMetrics displayMetrics = new DisplayMetrics();
        Context context = mWindowAndroid.getContext().get();
        WindowManager windowManager =
                (WindowManager) context.getSystemService(Context.WINDOW_SERVICE);
        windowManager.getDefaultDisplay().getMetrics(displayMetrics);

        int width = displayMetrics.widthPixels;
        int minSize =
                context.getResources().getDimensionPixelSize(R.dimen.photo_picker_tile_min_size);
        mPadding =
                mMagnifyingMode
                        ? 0
                        : context.getResources()
                                .getDimensionPixelSize(R.dimen.photo_picker_tile_gap);
        mColumns = mMagnifyingMode ? 1 : Math.max(1, (width - mPadding) / (minSize + mPadding));
        mImageWidth = (width - mPadding * (mColumns + 1)) / mColumns;
        if (!mMagnifyingMode) mSpecialTileHeight = mImageWidth;

        // Make sure columns and padding are either both even or both odd.
        if (!mMagnifyingMode && ((mColumns % 2) == 0) != ((mPadding % 2) == 0)) {
            mPadding++;
        }
    }

    /** Asynchronously enumerates bitmaps on disk. */
    private void enumerateBitmaps() {
        if (sTestFiles != null) {
            filesEnumeratedCallback(sTestFiles);
            return;
        }

        if (mWorkerTask != null) {
            mWorkerTask.cancel(true);
        }

        mEnumStartTime = SystemClock.elapsedRealtime();
        mWorkerTask =
                new FileEnumWorkerTask(
                        mWindowAndroid,
                        this,
                        new MimeTypeFilter(mMimeTypes, true),
                        mMimeTypes,
                        mContentResolver);
        mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /** Notifies any listeners that one or more photos have been selected. */
    private void notifyPhotosSelected() {
        List<PickerBitmap> selectedFiles = mSelectionDelegate.getSelectedItemsAsList();
        Collections.sort(selectedFiles);
        Uri[] photos = new Uri[selectedFiles.size()];
        int i = 0;
        for (PickerBitmap bitmap : selectedFiles) {
            photos[i++] = bitmap.getUri();
        }

        executeAction(
                PhotoPickerListener.PhotoPickerAction.PHOTOS_SELECTED, photos, ACTION_PHOTO_PICKED);
    }

    /** A class for implementing grid spacing between items. */
    private class GridSpacingItemDecoration extends RecyclerView.ItemDecoration {
        // The number of spans to account for.
        private int mSpanCount;

        // The amount of spacing to use.
        private int mSpacing;

        public GridSpacingItemDecoration(int spanCount, int spacing) {
            mSpanCount = spanCount;
            mSpacing = spacing;
        }

        @Override
        public void getItemOffsets(
                Rect outRect, View view, RecyclerView parent, RecyclerView.State state) {
            if (mMagnifyingMode) {
                outRect.set(0, 0, 0, mSpacing);
                return;
            }

            int left = 0;
            int right = 0;
            int top = 0;
            int bottom = 0;
            int position = parent.getChildAdapterPosition(view);

            if (position >= 0) {
                int column = position % mSpanCount;

                left = mSpacing - ((column * mSpacing) / mSpanCount);
                right = (column + 1) * mSpacing / mSpanCount;

                if (position < mSpanCount) {
                    top = mSpacing;
                }
                bottom = mSpacing;
            }

            outRect.set(left, top, right, bottom);
        }
    }

    /**
     * Report back what the user selected in the dialog, report UMA and clean up.
     *
     * @param action The action taken.
     * @param photos The photos that were selected (if any).
     * @param umaId The UMA value to record with the action.
     */
    private void executeAction(
            @PhotoPickerListener.PhotoPickerAction int action, Uri[] photos, int umaId) {
        mListener.onPhotoPickerUserAction(action, photos);
        if (mDialog != null) mDialog.dismiss();
        recordFinalUmaStats(umaId);
    }

    /**
     * Record UMA statistics (what action was taken in the dialog and other performance stats).
     *
     * @param action The action the user took in the dialog.
     */
    private void recordFinalUmaStats(int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PhotoPicker.DialogAction", action, ACTION_BOUNDARY);
        RecordHistogram.recordCount1MHistogram(
                "Android.PhotoPicker.DecodeRequests", mPickerAdapter.getDecodeRequestCount());
        RecordHistogram.recordCount1MHistogram(
                "Android.PhotoPicker.CacheHits", mPickerAdapter.getCacheHitCount());
    }

    /** Sets a list of files to use as data for the dialog. For testing use only. */
    @VisibleForTesting
    public static void setTestFiles(List<PickerBitmap> testFiles) {
        sTestFiles = new ArrayList<>(testFiles);
    }

    public SelectionDelegate<PickerBitmap> getSelectionDelegateForTesting() {
        return mSelectionDelegate;
    }

    public PickerVideoPlayer getVideoPlayerForTesting() {
        return mVideoPlayer;
    }
}
