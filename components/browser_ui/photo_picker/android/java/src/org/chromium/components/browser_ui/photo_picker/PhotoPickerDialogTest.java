// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.photo_picker;

import android.content.ContentResolver;
import android.content.Intent;
import android.content.res.Configuration;
import android.net.Uri;
import android.os.Build;
import android.os.StrictMode;
import android.provider.MediaStore;
import android.view.View;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.widget.Button;

import androidx.recyclerview.widget.RecyclerView;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.MathUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.TestAnimations.EnableAnimations;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.components.browser_ui.widget.RecyclerViewTestUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate.SelectionObserver;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TouchCommon;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.PhotoPickerListener;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.test.util.BlankUiTestActivityTestCase;
import org.chromium.ui.test.util.RenderTestRule;

import java.io.File;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.TimeUnit;

/** Tests for the PhotoPickerDialog class. */
@RunWith(BaseJUnit4ClassRunner.class)
public class PhotoPickerDialogTest extends BlankUiTestActivityTestCase
        implements PhotoPickerListener,
                SelectionObserver<PickerBitmap>,
                DecoderServiceHost.DecoderStatusCallback,
                PickerVideoPlayer.VideoPlaybackStatusCallback,
                AnimationListener {
    // The timeout (in seconds) to wait for the decoder service to be ready.
    private static final long WAIT_TIMEOUT_SECONDS = 30L;
    private static final long VIDEO_TIMEOUT_SECONDS = 10L;

    @Rule
    public RenderTestRule mRenderTestRule =
            RenderTestRule.Builder.withPublicCorpus()
                    .setBugComponent(RenderTestRule.Component.UI_BROWSER_MEDIA_PICKER)
                    .build();

    private WindowAndroid mWindowAndroid;

    // The dialog we are testing.
    private PhotoPickerDialog mDialog;

    // The data to show in the dialog (A map of filepath to last-modified time).
    // Map<String, Long> mTestFiles;
    private List<PickerBitmap> mTestFiles;

    // The selection delegate for the dialog.
    private SelectionDelegate<PickerBitmap> mSelectionDelegate;

    // The last action recorded in the dialog (e.g. photo selected).
    private @PhotoPickerAction int mLastActionRecorded;

    // The final set of photos picked by the dialog. Can be an empty array, if
    // nothing was selected.
    private Uri[] mLastSelectedPhotos;

    // A list of view IDs we receive from an animating event in the order the events occurred.
    private List<Long> mLastViewAnimatingIds = new ArrayList();

    // A list of view alpha values we receive from an animating event in the order the events
    // occurred.
    private List<Float> mLastViewAnimatingAlphas = new ArrayList();

    // The list of currently selected photos (built piecemeal).
    private List<PickerBitmap> mCurrentPhotoSelection;

    // True when {@link onPhotoPickerDismissed} has been called.
    private boolean mDismissed;

    // A callback that fires when something is selected in the dialog.
    public final CallbackHelper mOnSelectionCallback = new CallbackHelper();

    // A callback that fires when an action is taken in the dialog (cancel/done etc).
    public final CallbackHelper mOnActionCallback = new CallbackHelper();

    // A callback that fires when the decoder is ready.
    public final CallbackHelper mOnDecoderReadyCallback = new CallbackHelper();

    // A callback that fires when the decoder is idle.
    public final CallbackHelper mOnDecoderIdleCallback = new CallbackHelper();

    // A callback that fires when a PickerBitmapView is animated in the dialog.
    public final CallbackHelper mOnAnimatedCallback = new CallbackHelper();

    // A callback that fires when playback starts for a video.
    public final CallbackHelper mOnVideoPlayingCallback = new CallbackHelper();

    // A callback that fires when playback ends for a video.
    public final CallbackHelper mOnVideoEndedCallback = new CallbackHelper();

    // A callback that fires when overlay controls finish animating.
    public final CallbackHelper mOnVideoAnimationEndCallback = new CallbackHelper();

    @Before
    public void setUp() throws Exception {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();
        mWindowAndroid =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return new ActivityWindowAndroid(
                                    getActivity(),
                                    /* listenToActivityState= */ true,
                                    IntentRequestTracker.createFromActivity(getActivity()));
                        });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DecoderServiceHost.setIntentSupplier(
                            () -> {
                                return new Intent(getActivity(), TestImageDecoderService.class);
                            });
                });
        PickerVideoPlayer.setProgressCallback(this);
        PickerBitmapView.setAnimationListenerForTest(this);
        DecoderServiceHost.setStatusCallback(this);
    }

    @After
    public void tearDown() throws Exception {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mWindowAndroid.destroy();
                });
    }

    private void setupTestFiles() {
        mTestFiles = new ArrayList<>();
        mTestFiles.add(new PickerBitmap(Uri.parse("a"), 5L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("b"), 4L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("c"), 3L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("d"), 2L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("e"), 1L, PickerBitmap.TileTypes.PICTURE));
        mTestFiles.add(new PickerBitmap(Uri.parse("f"), 0L, PickerBitmap.TileTypes.PICTURE));
        PickerCategoryView.setTestFiles(mTestFiles);
    }

    private void setupTestFilesWith80ColoredSquares() {
        mTestFiles = new ArrayList<>();
        String green = "green100x100.jpg";
        String yellow = "yellow100x100.jpg";
        String red = "red100x100.jpg";
        String blue = "blue100x100.jpg";
        String filePath =
                UrlUtils.getIsolatedTestFilePath("chrome/test/data/android/photo_picker/");

        // The actual value of lastModified is not important, except that each entry must have a
        // unique lastModified stamp in order to ensure a stable order (tiles are ordered in
        // descending order by lastModified). Also, by decrementing this when adding entries (as
        // opposed to incrementing) the tiles will appear in same order as they are added.
        long lastModified = 1000;
        for (int i = 0; i < 50; ++i) {
            mTestFiles.add(
                    new PickerBitmap(
                            Uri.fromFile(new File(filePath + green)),
                            lastModified--,
                            PickerBitmap.TileTypes.PICTURE));
            mTestFiles.add(
                    new PickerBitmap(
                            Uri.fromFile(new File(filePath + yellow)),
                            lastModified--,
                            PickerBitmap.TileTypes.PICTURE));
            mTestFiles.add(
                    new PickerBitmap(
                            Uri.fromFile(new File(filePath + red)),
                            lastModified--,
                            PickerBitmap.TileTypes.PICTURE));
            mTestFiles.add(
                    new PickerBitmap(
                            Uri.fromFile(new File(filePath + blue)),
                            lastModified--,
                            PickerBitmap.TileTypes.PICTURE));
        }
        PickerCategoryView.setTestFiles(mTestFiles);
    }

    // PhotoPickerDialog.PhotoPickerListener:

    @Override
    public void onPhotoPickerUserAction(@PhotoPickerAction int action, Uri[] photos) {
        mLastActionRecorded = action;
        mLastSelectedPhotos = photos != null ? photos.clone() : null;
        if (mLastSelectedPhotos != null) Arrays.sort(mLastSelectedPhotos);
        mOnActionCallback.notifyCalled();
    }

    @Override
    public void onPhotoPickerDismissed() {
        Assert.assertFalse(mDismissed);
        mDismissed = true;
    }

    // DecoderServiceHost.DecoderStatusCallback:

    @Override
    public void serviceReady() {
        mOnDecoderReadyCallback.notifyCalled();
    }

    @Override
    public void decoderIdle() {
        mOnDecoderIdleCallback.notifyCalled();
    }

    // PickerCategoryView.VideoStatusCallback:

    @Override
    public void onVideoPlaying() {
        mOnVideoPlayingCallback.notifyCalled();
    }

    @Override
    public void onVideoEnded() {
        mOnVideoEndedCallback.notifyCalled();
    }

    @Override
    public void onAnimationStart(long viewId, float currentAlpha) {
        mLastViewAnimatingIds.add(viewId);
        mLastViewAnimatingAlphas.add(currentAlpha);
    }

    @Override
    public void onAnimationCancel(long viewId, float currentAlpha) {
        mLastViewAnimatingIds.add(viewId);
        mLastViewAnimatingAlphas.add(currentAlpha);
    }

    @Override
    public void onAnimationEnd(long viewId, float currentAlpha) {
        mLastViewAnimatingIds.add(viewId);
        mLastViewAnimatingAlphas.add(currentAlpha);

        mOnVideoAnimationEndCallback.notifyCalled();
    }

    // SelectionObserver:

    @Override
    public void onSelectionStateChange(List<PickerBitmap> photosSelected) {
        mCurrentPhotoSelection = new ArrayList<>(photosSelected);
        mOnSelectionCallback.notifyCalled();
    }

    // AnimationListener:
    @Override
    public void onAnimationStart(Animation animation) {}

    @Override
    public void onAnimationEnd(Animation animation) {
        mOnAnimatedCallback.notifyCalled();
    }

    @Override
    public void onAnimationRepeat(Animation animation) {}

    private RecyclerView getRecyclerView() {
        return (RecyclerView) mDialog.findViewById(R.id.selectable_list_recycler_view);
    }

    private PhotoPickerDialog createDialogWithContentResolver(
            final ContentResolver contentResolver,
            final boolean multiselect,
            final List<String> mimeTypes)
            throws Exception {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final PhotoPickerDialog dialog =
                            new PhotoPickerDialog(
                                    mWindowAndroid,
                                    contentResolver,
                                    PhotoPickerDialogTest.this,
                                    multiselect,
                                    mimeTypes);
                    dialog.show();
                    mSelectionDelegate =
                            dialog.getCategoryViewForTesting().getSelectionDelegateForTesting();
                    if (!multiselect) mSelectionDelegate.setSingleSelectionMode();
                    mSelectionDelegate.addObserver(this);
                    mDialog = dialog;
                    return dialog;
                });
    }

    private PhotoPickerDialog createDialog(final boolean multiselect, final List<String> mimeTypes)
            throws Exception {
        return createDialogWithContentResolver(
                getActivity().getContentResolver(), multiselect, mimeTypes);
    }

    private void waitForDecoder() throws Exception {
        int callCount = mOnDecoderReadyCallback.getCallCount();
        mOnDecoderReadyCallback.waitForCallback(
                callCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void waitForDecoderIdle() throws Exception {
        int callCount = mOnDecoderIdleCallback.getCallCount();
        mOnDecoderIdleCallback.waitForCallback(
                callCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void clickView(final int position, final int expectedSelectionCount) throws Exception {
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.waitForView(recyclerView, position);

        int callCount = mOnSelectionCallback.getCallCount();
        TouchCommon.singleClickView(
                recyclerView.findViewHolderForAdapterPosition(position).itemView);
        mOnSelectionCallback.waitForCallback(callCount, 1);

        // Validate the correct selection took place.
        Assert.assertEquals(expectedSelectionCount, mCurrentPhotoSelection.size());
        Assert.assertTrue(mSelectionDelegate.isItemSelected(mTestFiles.get(position)));
    }

    private void clickDone() throws Exception {
        mLastActionRecorded = PhotoPickerAction.NUM_ENTRIES;

        PhotoPickerToolbar toolbar = (PhotoPickerToolbar) mDialog.findViewById(R.id.action_bar);
        Button done = (Button) toolbar.findViewById(R.id.done);
        int callCount = mOnActionCallback.getCallCount();
        TouchCommon.singleClickView(done);
        mOnActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mDismissed;
                        }));
    }

    private void clickCancel() throws Exception {
        mLastActionRecorded = PhotoPickerAction.NUM_ENTRIES;

        PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();
        View cancel = new View(getActivity());
        int callCount = mOnActionCallback.getCallCount();
        categoryView.onClick(cancel);
        mOnActionCallback.waitForCallback(callCount, 1);
        Assert.assertEquals(PhotoPickerAction.CANCEL, mLastActionRecorded);
        Assert.assertTrue(
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return mDismissed;
                        }));
    }

    private void playVideo(Uri uri) throws Exception {
        int callCount = mOnVideoPlayingCallback.getCallCount();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog.getCategoryViewForTesting().startVideoPlaybackAsync(uri);
                });
        mOnVideoPlayingCallback.waitForCallback(
                callCount, 1, VIDEO_TIMEOUT_SECONDS, TimeUnit.SECONDS);
    }

    private void dismissDialog() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog.dismiss();
                    Assert.assertTrue(mDismissed);
                });
    }

    /**
     * Tests what happens when the ContentResolver returns a null cursor when query() is called (a
     * regression test for https://crbug.com/1072415). Note: This test does not call
     * setupTestFiles() so that the real FileEnumWorkerTask is used.
     */
    @Test
    @LargeTest
    public void testNoCrashWhenContentResolverQueryReturnsNull() throws Throwable {
        ContentResolver contentResolver = Mockito.mock(ContentResolver.class);
        Uri contentUri = MediaStore.Files.getContentUri("external");
        Mockito.doReturn(null)
                .when(contentResolver)
                .query(contentUri, new String[] {}, "", new String[] {}, "");

        createDialogWithContentResolver(
                contentResolver, false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // The test should not have crashed at this point, as per https://crbug.com/1072415,
        // so the loading should have aborted (gracefully) because the image cursor could not be
        // constructed.
        dismissDialog();
    }

    @Test
    @LargeTest
    public void testNoSelection() throws Throwable {
        setupTestFiles();
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        int expectedSelectionCount = 1;
        clickView(0, expectedSelectionCount);
        clickCancel();

        Assert.assertNull(mLastSelectedPhotos);
        Assert.assertEquals(PhotoPickerAction.CANCEL, mLastActionRecorded);
    }

    @Test
    @LargeTest
    public void testSingleSelectionPhoto() throws Throwable {
        setupTestFiles();
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // Expected selection count is 1 because clicking on a new view unselects other.
        int expectedSelectionCount = 1;

        // Click the first view.
        int callCount = mOnAnimatedCallback.getCallCount();
        clickView(0, expectedSelectionCount);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        // Click the second view.
        callCount = mOnAnimatedCallback.getCallCount();
        clickView(1, expectedSelectionCount);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        clickDone();

        Assert.assertEquals(1, mLastSelectedPhotos.length);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(mTestFiles.get(1).getUri().getPath(), mLastSelectedPhotos[0].getPath());
    }

    @Test
    @LargeTest
    public void testBackPressDismiss() throws Throwable {
        setupTestFiles();
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // Expected selection count is 1 because clicking on a new view unselects other.
        int expectedSelectionCount = 1;

        // Click the first view.
        int callCount = mOnAnimatedCallback.getCallCount();
        clickView(0, expectedSelectionCount);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        // Click the second view.
        callCount = mOnAnimatedCallback.getCallCount();
        clickView(1, expectedSelectionCount);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDialog.getOnBackPressedDispatcher().onBackPressed();
                });

        Assert.assertNull(mLastSelectedPhotos);
        Assert.assertEquals(PhotoPickerAction.CANCEL, mLastActionRecorded);
        Assert.assertFalse(mDialog.isShowing());
    }

    @Test
    @LargeTest
    public void testMultiSelectionPhoto() throws Throwable {
        setupTestFiles();
        createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
        Assert.assertTrue(mDialog.isShowing());
        waitForDecoder();

        // Multi-selection is enabled, so each click is counted.
        int expectedSelectionCount = 1;

        // Click first view.
        int callCount = mOnAnimatedCallback.getCallCount();
        clickView(0, expectedSelectionCount++);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        // Click third view.
        callCount = mOnAnimatedCallback.getCallCount();
        clickView(2, expectedSelectionCount++);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        // Click fifth view.
        callCount = mOnAnimatedCallback.getCallCount();
        clickView(4, expectedSelectionCount++);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        clickDone();

        Assert.assertEquals(3, mLastSelectedPhotos.length);
        Assert.assertEquals(PhotoPickerAction.PHOTOS_SELECTED, mLastActionRecorded);
        Assert.assertEquals(mTestFiles.get(0).getUri().getPath(), mLastSelectedPhotos[0].getPath());
        Assert.assertEquals(mTestFiles.get(2).getUri().getPath(), mLastSelectedPhotos[1].getPath());
        Assert.assertEquals(mTestFiles.get(4).getUri().getPath(), mLastSelectedPhotos[2].getPath());
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O) // Video is only supported on O+.
    public void testVideoPlayerPlayAndRestart() throws Throwable {
        // Requesting to play a video is not a case of an accidental disk read on the UI thread.
        StrictMode.ThreadPolicy oldPolicy =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return StrictMode.allowThreadDiskReads();
                        });

        try {
            setupTestFiles();
            createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
            Assert.assertTrue(mDialog.isShowing());
            waitForDecoder();

            PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();

            View container = categoryView.findViewById(R.id.playback_container);
            Assert.assertTrue(container.getVisibility() == View.GONE);

            // This test video takes one second to play.
            String fileName = "chrome/test/data/android/photo_picker/noogler_1sec.mp4";
            File file = new File(UrlUtils.getIsolatedTestFilePath(fileName));

            int callCount = mOnVideoEndedCallback.getCallCount();

            playVideo(Uri.fromFile(file));
            Assert.assertTrue(container.getVisibility() == View.VISIBLE);

            mOnVideoEndedCallback.waitForCallback(callCount, 1);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        View mute = categoryView.findViewById(R.id.mute);
                        categoryView.getVideoPlayerForTesting().onClick(mute);
                    });

            // Clicking the play button should restart playback.
            callCount = mOnVideoEndedCallback.getCallCount();

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        View playbutton = categoryView.findViewById(R.id.video_player_play_button);
                        categoryView.getVideoPlayerForTesting().onClick(playbutton);
                    });

            mOnVideoEndedCallback.waitForCallback(callCount, 1);

            dismissDialog();
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        StrictMode.setThreadPolicy(oldPolicy);
                    });
        }
    }

    @Test
    @LargeTest
    @MinAndroidSdkLevel(Build.VERSION_CODES.O) // Video is only supported on O+.
    public void testVideoPlayerPlayAndBackPress() throws Throwable {
        // Requesting to play a video is not a case of an accidental disk read on the UI thread.
        StrictMode.ThreadPolicy oldPolicy =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return StrictMode.allowThreadDiskReads();
                        });

        try {
            setupTestFiles();
            createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
            Assert.assertTrue(mDialog.isShowing());
            waitForDecoder();

            PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();

            View container = categoryView.findViewById(R.id.playback_container);
            Assert.assertTrue(container.getVisibility() == View.GONE);

            // This test video takes one second to play.
            String fileName = "chrome/test/data/android/photo_picker/noogler_1sec.mp4";
            File file = new File(UrlUtils.getIsolatedTestFilePath(fileName));

            int callCount = mOnVideoEndedCallback.getCallCount();

            playVideo(Uri.fromFile(file));
            Assert.assertTrue(container.getVisibility() == View.VISIBLE);

            mOnVideoEndedCallback.waitForCallback(callCount, 1);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mDialog.getOnBackPressedDispatcher().onBackPressed();
                    });

            // Clicking the play button should restart playback.
            callCount = mOnVideoEndedCallback.getCallCount();

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        View playbutton = categoryView.findViewById(R.id.video_player_play_button);
                        categoryView.getVideoPlayerForTesting().onClick(playbutton);
                    });

            mOnVideoEndedCallback.waitForCallback(callCount, 1);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        mDialog.getOnBackPressedDispatcher().onBackPressed();
                    });
            Assert.assertTrue(mDismissed);
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        StrictMode.setThreadPolicy(oldPolicy);
                    });
        }
    }

    private void verifyVisible(int viewId, int eventId) {
        Assert.assertEquals(
                "Unexpected view ID for event " + eventId,
                viewId,
                (long) mLastViewAnimatingIds.get(eventId));
        Assert.assertEquals(
                "Unexpected alpha value for event " + eventId,
                1.0f,
                (double) mLastViewAnimatingAlphas.get(eventId),
                MathUtils.EPSILON);
    }

    private void verifyHidden(int viewId, int eventId) {
        Assert.assertEquals(
                "Unexpected view ID for event " + eventId,
                viewId,
                (long) mLastViewAnimatingIds.get(eventId));
        Assert.assertEquals(
                "Unexpected alpha value for event " + eventId,
                0.0f,
                (double) mLastViewAnimatingAlphas.get(eventId),
                MathUtils.EPSILON);
    }

    @Test
    @LargeTest
    @EnableAnimations
    @MinAndroidSdkLevel(Build.VERSION_CODES.O) // Video is only supported on O+.
    @DisableIf.Build(supported_abis_includes = "x86", message = "https://crbug.com/1092104")
    @DisableIf.Build(supported_abis_includes = "x86_64", message = "https://crbug.com/1092104")
    @DisabledTest(message = "https://crbug.com/1311783")
    public void testVideoPlayerAnimations() throws Throwable {
        PickerVideoPlayer.setShortAnimationTimesForTesting(true);

        // Requesting to play a video is not a case of an accidental disk read on the UI thread.
        StrictMode.ThreadPolicy oldPolicy =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return StrictMode.allowThreadDiskReads();
                        });

        try {
            setupTestFiles();
            createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
            Assert.assertTrue(mDialog.isShowing());
            waitForDecoder();

            PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();

            View container = categoryView.findViewById(R.id.playback_container);
            Assert.assertTrue(container.getVisibility() == View.GONE);

            String fileName = "chrome/test/data/android/photo_picker/noogler_1sec.mp4";
            File file = new File(UrlUtils.getIsolatedTestFilePath(fileName));

            int callCount = mOnVideoAnimationEndCallback.getCallCount();

            playVideo(Uri.fromFile(file));
            Assert.assertTrue(container.getVisibility() == View.VISIBLE);

            // This keeps track of event ordering.
            int i = 0;

            // Wait for two animation sets (until the controls and play button have animated away).
            mOnVideoAnimationEndCallback.waitForCallback(callCount, 2);

            // All controls start off showing when the video starts playing, and animations will
            // start to fade them away: one animation for the video controls and a separate one for
            // the Play/Pause button. Play button is the first button to disappear (shortest start
            // time and duration) and shortly thereafter the video controls start disappearing.
            verifyVisible(R.id.video_player_play_button, i++);
            verifyHidden(R.id.video_player_play_button, i++);
            verifyVisible(R.id.video_controls, i++);
            verifyHidden(R.id.video_controls, i++);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        // Single-tapping should make the controls visible again and then fade away.
                        categoryView.getVideoPlayerForTesting().singleTapForTesting();
                    });

            // Animation-end has been called twice now, expect four more calls after single-tapping
            // because controls fade in and then fade out again.
            callCount += 2;
            mOnVideoAnimationEndCallback.waitForCallback(callCount, 4);

            // The controls and the Play button start animating into view at the same time but the
            // Play button is quicker to appear.
            verifyHidden(R.id.video_controls, i++);
            verifyHidden(R.id.video_player_play_button, i++);
            verifyVisible(R.id.video_player_play_button, i++);
            verifyVisible(R.id.video_controls, i++);

            // After a short while, the controls disappear again (with same delay and duration).
            verifyVisible(R.id.video_controls, i++);
            verifyVisible(R.id.video_player_play_button, i++);
            verifyHidden(R.id.video_controls, i++);
            verifyHidden(R.id.video_player_play_button, i++);

            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        // Double-tapping left of screen will cause the video to roll back to the
                        // beginning and controls to be shown immediately (no fade-in) and then
                        // gradually fade out.
                        categoryView.getVideoPlayerForTesting().doubleTapForTesting(/* x= */ 0f);
                    });

            callCount += 4;
            mOnVideoAnimationEndCallback.waitForCallback(callCount, 2);

            // Controls will show without animation, but should fade away (play fades out first).
            verifyVisible(R.id.video_player_play_button, i++);
            verifyHidden(R.id.video_player_play_button, i++);
            verifyVisible(R.id.video_controls, i++);
            verifyHidden(R.id.video_controls, i++);

            dismissDialog();
        } finally {
            ThreadUtils.runOnUiThreadBlocking(
                    () -> {
                        StrictMode.setThreadPolicy(oldPolicy);
                    });
        }
    }

    @Test
    @LargeTest
    public void testOrientationChanges() throws Throwable {
        setupTestFiles();
        createDialog(true, Arrays.asList("image/*")); // Multi-select = true.
        Assert.assertTrue(mDialog.isShowing());

        int callCount = mOnDecoderReadyCallback.getCallCount();

        // Simulate an early configuration change for the photo grid.
        Configuration configuration = getActivity().getResources().getConfiguration();
        PickerCategoryView categoryView = mDialog.getCategoryViewForTesting();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    categoryView.onConfigurationChanged(configuration);
                });

        mOnDecoderReadyCallback.waitForCallback(
                callCount, 1, WAIT_TIMEOUT_SECONDS, TimeUnit.SECONDS);

        // Simulate an early configuration change for the video player (before showing).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PickerVideoPlayer videoPlayer = categoryView.getVideoPlayerForTesting();
                    videoPlayer.onConfigurationChanged(configuration);
                });

        dismissDialog();
    }

    @Test
    @LargeTest
    @Feature("RenderTest")
    public void testBorderPersistence() throws Exception {
        setupTestFilesWith80ColoredSquares();
        createDialog(false, Arrays.asList("image/*")); // Multi-select = false.
        waitForDecoderIdle();

        mRenderTestRule.render(mDialog.getCategoryViewForTesting(), "initial_load");

        // Click the first view.
        int expectedSelectionCount = 1;
        int callCount = mOnAnimatedCallback.getCallCount();
        clickView(0, expectedSelectionCount);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        mRenderTestRule.render(mDialog.getCategoryViewForTesting(), "first_view_clicked");

        // Now test that you can scroll the image out of view and back in again, and the selection
        // border should be maintained.
        RecyclerView recyclerView = getRecyclerView();
        RecyclerViewTestUtils.scrollToBottom(recyclerView);

        callCount = mOnAnimatedCallback.getCallCount();
        RecyclerViewTestUtils.scrollToView(recyclerView, 0);
        mOnAnimatedCallback.waitForCallback(callCount, 1);

        mRenderTestRule.render(mDialog.getCategoryViewForTesting(), "first_view_clicked");

        dismissDialog();
    }
}
