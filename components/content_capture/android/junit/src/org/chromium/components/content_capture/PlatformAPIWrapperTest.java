// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.content_capture;

import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.when;

import android.graphics.Rect;
import android.view.ViewStructure;
import android.view.autofill.AutofillId;
import android.view.contentcapture.ContentCaptureSession;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatchers;
import org.mockito.InOrder;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

import java.util.ArrayList;
import java.util.Arrays;

/**
 * The unit tests for PlatformAPIWrapper, NotificationTask and its subclasses. It runs the
 * ContentCapturedTask, ContentUpdateTask, ContentRemovedTask and SessionRemovedTask, then verifies
 * if the PlatformAPIWrapper get the expected parameters.
 *
 * <p>This test also verifies that the covered code path won't call ContentCaptureSession API
 * directly, and verified that all tasks catch the PlatformAPIException correctly.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class PlatformAPIWrapperTest {
    /**
     * This class implements the methods we care, mockito will mock other method for us. By
     * declaring the method be final, it won't be mocked by mockito.
     */
    private abstract static class ViewStructureTestHelper extends ViewStructure {
        private CharSequence mText;
        private Rect mDimens;

        @Override
        public final void setText(CharSequence text) {
            mText = text;
        }

        @Override
        public final CharSequence getText() {
            return mText;
        }

        @Override
        public final void setDimens(
                int left, int top, int scrollX, int scrollY, int width, int height) {
            mDimens = new Rect(left, top, left + width, top + height);
        }

        public final Rect getDimens() {
            return mDimens;
        }
    }

    /**
     * This class records the invoked method and the passing parameter in sequence for verification
     * later.
     */
    private static class PlatformAPIWrapperTestHelper extends PlatformAPIWrapper {
        // The Id of API call.
        public static final int CREATE_CONTENT_CAPTURE_SESSION = 1;
        public static final int NEW_AUTOFILL_ID = 2;
        public static final int NEW_VIRTUAL_VIEW_STRUCTURE = 3;
        public static final int NOTIFY_VIEW_APPEARED = 4;
        public static final int NOTIFY_VIEW_DISAPPEARED = 5;
        public static final int NOTIFY_VIEWS_DISAPPEARED = 6;
        public static final int NOTIFY_VIEW_TEXT_CHANGED = 7;
        public static final int DESTROY_CONTENT_CAPTURE_SESSION = 8;
        public static final int NOTIFY_FAVICON_UPDATE = 9;

        // The array for objects returned by the mocked APIs
        public final ArrayList<ContentCaptureSession> mCreatedContentCaptureSessions =
                new ArrayList<ContentCaptureSession>();
        public final ArrayList<AutofillId> mCreatedAutofilIds = new ArrayList<AutofillId>();
        public final ArrayList<ViewStructureTestHelper> mCreatedViewStructures =
                new ArrayList<ViewStructureTestHelper>();
        public final ArrayList<AutofillId> mCreatedViewStructuresAutofilIds =
                new ArrayList<AutofillId>();

        // Array to record the API calls in sequence.
        private volatile ArrayList<Integer> mCallbacks = new ArrayList<Integer>();

        // Mock a ContentCaptureSession which will throw a exception if all its public
        // method is called because this test use the mocked PlatformAPIWrapper, the methods
        // of ContentCaptureSession will never be called.
        private ContentCaptureSession createMockedContentCaptureSession() {
            ContentCaptureSession mockedContentCaptureSession =
                    Mockito.mock(ContentCaptureSession.class);
            // Prevent the below methods being called from the class other than PlatformAPIWrapper.
            final String errorMsg = "Shall be called from PlatformAPIWrapper.";
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .createContentCaptureSession(ArgumentMatchers.any());
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .newAutofillId(ArgumentMatchers.any(), ArgumentMatchers.anyLong());
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .destroy();
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .getContentCaptureContext();
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .getContentCaptureSessionId();
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .newViewStructure(ArgumentMatchers.any());
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .newVirtualViewStructure(ArgumentMatchers.any(), ArgumentMatchers.anyLong());
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .notifyViewAppeared(ArgumentMatchers.any());
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .notifyViewDisappeared(ArgumentMatchers.any());
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .notifyViewTextChanged(ArgumentMatchers.any(), ArgumentMatchers.any());
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .notifyViewsDisappeared(ArgumentMatchers.any(), ArgumentMatchers.any());
            doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
                    .when(mockedContentCaptureSession)
                    .setContentCaptureContext(ArgumentMatchers.any());
            // TODO(crbug.com/40156022): Enable below once mockito support them.
            // doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
            //        .when(mockedContentCaptureSession)
            //        .notifySessionPaused();
            // doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
            //        .when(mockedContentCaptureSession)
            //        .notifySessionResumed();
            // doThrow(new RuntimeException("Shall be called from PlatformAPIWrapper."))
            //        .when(mockedContentCaptureSession)
            //        .notifyViewInsetsChanged(ArgumentMatchers.any());
            return mockedContentCaptureSession;
        }

        @Override
        public ContentCaptureSession createContentCaptureSession(
                ContentCaptureSession parent, String url, String favicon) {
            mCallbacks.add(CREATE_CONTENT_CAPTURE_SESSION);
            ContentCaptureSession mockedContentCaptureSession = createMockedContentCaptureSession();
            mCreatedContentCaptureSessions.add(mockedContentCaptureSession);
            return mockedContentCaptureSession;
        }

        @Override
        public void destroyContentCaptureSession(ContentCaptureSession session) {
            mCallbacks.add(DESTROY_CONTENT_CAPTURE_SESSION);
        }

        @Override
        public AutofillId newAutofillId(
                ContentCaptureSession parent, AutofillId rootAutofillId, long id) {
            mCallbacks.add(NEW_AUTOFILL_ID);
            AutofillId mockedAutofillId = Mockito.mock(AutofillId.class);
            mCreatedAutofilIds.add(mockedAutofillId);
            return mockedAutofillId;
        }

        @Override
        public ViewStructure newVirtualViewStructure(
                ContentCaptureSession parent, AutofillId parentAutofillId, long id) {
            mCallbacks.add(NEW_VIRTUAL_VIEW_STRUCTURE);
            ViewStructureTestHelper mockedViewStructure =
                    Mockito.mock(ViewStructureTestHelper.class);
            AutofillId mockedAutofilId = Mockito.mock(AutofillId.class);
            mCreatedViewStructuresAutofilIds.add(mockedAutofilId);
            when(mockedViewStructure.getAutofillId()).thenReturn(mockedAutofilId);
            mCreatedViewStructures.add(mockedViewStructure);
            return mockedViewStructure;
        }

        @Override
        public void notifyViewAppeared(ContentCaptureSession session, ViewStructure viewStructure) {
            mCallbacks.add(NOTIFY_VIEW_APPEARED);
        }

        @Override
        public void notifyViewDisappeared(ContentCaptureSession parent, AutofillId autofillId) {
            mCallbacks.add(NOTIFY_VIEW_DISAPPEARED);
        }

        @Override
        public void notifyViewsDisappeared(
                ContentCaptureSession session, AutofillId autofillId, long[] ids) {
            mCallbacks.add(NOTIFY_VIEWS_DISAPPEARED);
        }

        @Override
        public void notifyViewTextChanged(
                ContentCaptureSession session, AutofillId autofillId, String newContent) {
            mCallbacks.add(NOTIFY_VIEW_TEXT_CHANGED);
        }

        @Override
        public void notifyFaviconUpdated(ContentCaptureSession session, String favicon) {
            mCallbacks.add(NOTIFY_FAVICON_UPDATE);
        }

        public void reset() {
            mCallbacks.clear();
        }

        public int[] getCallbacks() {
            int[] result = new int[mCallbacks.size()];
            int index = 0;
            for (Integer c : mCallbacks) {
                result[index++] = c;
            }
            return result;
        }
    }

    private static final String MAIN_URL = "http://main.domain.com";
    private static final String MAIN_TITLE = "MAIN TITLE";
    private static final String FAVICON =
            "[{" + "\"url\":\"http://main.domain.com/favicon\"," + "\"type:\":\"favicon\"" + "}]";
    private static final String UPDATED_FAVICON =
            "[{"
                    + "\"url\":\"http://main.domain.com/favicon\","
                    + "\"type:\":\"updated_favicon\""
                    + "}]";
    private static final String UPDATED_MAIN_TITLE = "MAIN TITLE UPDATE";
    private static final long MAIN_ID = 4;
    private static final Rect MAIN_FRAME_RECT = new Rect(0, 0, 200, 200);
    private static final String CHILD_URL = "http://test.domain.com";
    private static final String CHILD_TITLE = null;
    private static final long CHILD_FRAME_ID = 1;
    private static final Rect CHILD_FRAME_RECT = new Rect(0, 0, 100, 100);
    private static final long CHILD1_ID = 2;
    private static final String CHILD1_TEXT = "Hello";
    private static final Rect CHILD1_RECT = new Rect(10, 10, 20, 20);
    private static final long CHILD2_ID = 3;
    private static final String CHILD2_TEXT = "World";
    private static final Rect CHILD2_RECT = new Rect(20, 20, 20, 20);
    private static final String CHILD2_NEW_TEXT = " world!";
    private static final long[] REMOVED_IDS = {CHILD1_ID, CHILD2_ID};

    private ContentCaptureSession mMockedRootContentCaptureSession;
    private AutofillId mMockedRootAutofillId;
    private PlatformSession mRootPlatformSession;
    private PlatformAPIWrapperTestHelper mPlatformAPIWrapperTestHelper;
    private PlatformAPIWrapperTestHelper mPlatformAPIWrapperTestHelperSpy;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMockedRootContentCaptureSession = Mockito.mock(ContentCaptureSession.class);
        mMockedRootAutofillId = Mockito.mock(AutofillId.class);
        mRootPlatformSession =
                new PlatformSession(mMockedRootContentCaptureSession, mMockedRootAutofillId);
        mPlatformAPIWrapperTestHelper = new PlatformAPIWrapperTestHelper();
        mPlatformAPIWrapperTestHelperSpy = spy(mPlatformAPIWrapperTestHelper);
        PlatformAPIWrapper.setPlatformAPIWrapperImplForTesting(mPlatformAPIWrapperTestHelperSpy);
    }

    private static void verifyCallbacks(int[] expectedCallbacks, int[] results) {
        Assert.assertArrayEquals(
                "Expect: "
                        + Arrays.toString(expectedCallbacks)
                        + " Result: "
                        + Arrays.toString(results),
                expectedCallbacks,
                results);
    }

    private static int[] toIntArray(int... callbacks) {
        return callbacks;
    }

    private void runTaskAndVerifyCallback(NotificationTask task, int[] expectedCallbacks)
            throws Exception {
        mPlatformAPIWrapperTestHelper.reset();
        task.doInBackground();
        verifyCallbacks(expectedCallbacks, mPlatformAPIWrapperTestHelper.getCallbacks());
    }

    private void runTaskAndVerifyCallbackWithException(
            NotificationTask task, int[] expectedCallbacks) throws Exception {
        runTaskAndVerifyCallback(task, expectedCallbacks);
        Assert.assertTrue(task.hasPlatformExceptionForTesting());
    }

    private void verifyVirtualViewStructure(String expectedText, Rect expectedRect, int index) {
        Assert.assertEquals(
                "Index:" + index,
                expectedText,
                mPlatformAPIWrapperTestHelper.mCreatedViewStructures.get(index).getText());
        Assert.assertEquals(
                "Index:" + index,
                expectedRect,
                mPlatformAPIWrapperTestHelper.mCreatedViewStructures.get(index).getDimens());
    }

    private FrameSession createFrameSession() {
        FrameSession frameSession = new FrameSession(1);
        frameSession.add(
                ContentCaptureFrame.createContentCaptureFrame(
                        MAIN_ID,
                        MAIN_URL,
                        MAIN_FRAME_RECT.left,
                        MAIN_FRAME_RECT.top,
                        MAIN_FRAME_RECT.width(),
                        MAIN_FRAME_RECT.height(),
                        MAIN_TITLE,
                        FAVICON));
        return frameSession;
    }

    private FrameSession createFrameSessionForRemoveTask() {
        FrameSession frameSessionForRemoveTask = createFrameSession();
        frameSessionForRemoveTask.add(
                0,
                ContentCaptureFrame.createContentCaptureFrame(
                        CHILD_FRAME_ID,
                        CHILD_URL,
                        CHILD_FRAME_RECT.left,
                        CHILD_FRAME_RECT.top,
                        CHILD_FRAME_RECT.width(),
                        CHILD_FRAME_RECT.height(),
                        CHILD_TITLE,
                        null));
        return frameSessionForRemoveTask;
    }

    // The below createFooTask() create the tasks for tests.
    private ContentCapturedTask createContentCapturedTask() {
        FrameSession frameSession = createFrameSession();
        ContentCaptureFrame data =
                ContentCaptureFrame.createContentCaptureFrame(
                        CHILD_FRAME_ID,
                        CHILD_URL,
                        CHILD_FRAME_RECT.left,
                        CHILD_FRAME_RECT.top,
                        CHILD_FRAME_RECT.width(),
                        CHILD_FRAME_RECT.height(),
                        CHILD_TITLE,
                        null);
        ContentCaptureData.createContentCaptureData(
                data,
                CHILD1_ID,
                CHILD1_TEXT,
                CHILD1_RECT.left,
                CHILD1_RECT.top,
                CHILD1_RECT.width(),
                CHILD1_RECT.height());
        ContentCaptureData.createContentCaptureData(
                data,
                CHILD2_ID,
                CHILD2_TEXT,
                CHILD2_RECT.left,
                CHILD2_RECT.top,
                CHILD2_RECT.width(),
                CHILD2_RECT.height());
        return new ContentCapturedTask(frameSession, data, mRootPlatformSession);
    }

    private ContentUpdateTask createChild2ContentUpdateTask() {
        // Modifies child2
        ContentCaptureFrame changeTextData =
                ContentCaptureFrame.createContentCaptureFrame(
                        CHILD_FRAME_ID,
                        CHILD_URL,
                        CHILD_FRAME_RECT.left,
                        CHILD_FRAME_RECT.top,
                        CHILD_FRAME_RECT.width(),
                        CHILD_FRAME_RECT.height(),
                        CHILD_TITLE,
                        null);
        ContentCaptureData.createContentCaptureData(
                changeTextData,
                CHILD2_ID,
                CHILD2_NEW_TEXT,
                CHILD2_RECT.left,
                CHILD2_RECT.top,
                CHILD2_RECT.width(),
                CHILD2_RECT.height());
        return new ContentUpdateTask(createFrameSession(), changeTextData, mRootPlatformSession);
    }

    private ContentRemovedTask createRemoveChildrenTask() {
        // Removes the child1 and child2
        return new ContentRemovedTask(
                createFrameSessionForRemoveTask(), REMOVED_IDS, mRootPlatformSession);
    }

    private SessionRemovedTask createSessionRemovedTask() {
        return new SessionRemovedTask(createFrameSessionForRemoveTask(), mRootPlatformSession);
    }

    private TitleUpdateTask createTitleUpdateTask() {
        ContentCaptureFrame mainFrame =
                ContentCaptureFrame.createContentCaptureFrame(
                        MAIN_ID,
                        MAIN_URL,
                        MAIN_FRAME_RECT.left,
                        MAIN_FRAME_RECT.top,
                        MAIN_FRAME_RECT.width(),
                        MAIN_FRAME_RECT.height(),
                        UPDATED_MAIN_TITLE,
                        null);
        return new TitleUpdateTask(mainFrame, mRootPlatformSession);
    }

    private FaviconUpdateTask createFaviconUpdateTask() {
        FrameSession frameSession = new FrameSession(1);
        frameSession.add(
                ContentCaptureFrame.createContentCaptureFrame(
                        MAIN_ID,
                        MAIN_URL,
                        MAIN_FRAME_RECT.left,
                        MAIN_FRAME_RECT.top,
                        MAIN_FRAME_RECT.width(),
                        MAIN_FRAME_RECT.height(),
                        UPDATED_MAIN_TITLE,
                        UPDATED_FAVICON));
        return new FaviconUpdateTask(frameSession, mRootPlatformSession);
    }

    private void runContentCapturedTask() throws Exception {
        runTaskAndVerifyCallback(
                createContentCapturedTask(),
                toIntArray(
                        PlatformAPIWrapperTestHelper.CREATE_CONTENT_CAPTURE_SESSION,
                        PlatformAPIWrapperTestHelper.NEW_VIRTUAL_VIEW_STRUCTURE,
                        PlatformAPIWrapperTestHelper.NOTIFY_VIEW_APPEARED,
                        PlatformAPIWrapperTestHelper.CREATE_CONTENT_CAPTURE_SESSION,
                        PlatformAPIWrapperTestHelper.NEW_VIRTUAL_VIEW_STRUCTURE,
                        PlatformAPIWrapperTestHelper.NOTIFY_VIEW_APPEARED,
                        PlatformAPIWrapperTestHelper.NEW_VIRTUAL_VIEW_STRUCTURE,
                        PlatformAPIWrapperTestHelper.NOTIFY_VIEW_APPEARED,
                        PlatformAPIWrapperTestHelper.NEW_VIRTUAL_VIEW_STRUCTURE,
                        PlatformAPIWrapperTestHelper.NOTIFY_VIEW_APPEARED));
    }

    private NullPointerException createMainContentCaptureSessionException() {
        NullPointerException e =
                new NullPointerException() {
                    @Override
                    public StackTraceElement[] getStackTrace() {
                        StackTraceElement[] stack =
                                new StackTraceElement[super.getStackTrace().length + 1];
                        int index = 0;
                        for (StackTraceElement s : super.getStackTrace()) {
                            stack[index++] = s;
                        }
                        stack[index] =
                                new StackTraceElement(
                                        "android.view.contentcapture.MainContentCaptureSession",
                                        "sendEvent",
                                        "MainContentCaptureSession.java",
                                        349);
                        return stack;
                    }
                };
        return e;
    }

    @Test
    public void testTypicalLifecycle() throws Throwable {
        runContentCapturedTask();
        // Verifies main frame.
        InOrder inOrder = Mockito.inOrder(mPlatformAPIWrapperTestHelperSpy);
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .createContentCaptureSession(mMockedRootContentCaptureSession, MAIN_URL, FAVICON);

        // Verifies the ViewStructure for the main frame.
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .newVirtualViewStructure(
                        mMockedRootContentCaptureSession, mMockedRootAutofillId, MAIN_ID);
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .notifyViewAppeared(
                        mMockedRootContentCaptureSession,
                        mPlatformAPIWrapperTestHelper.mCreatedViewStructures.get(0));
        verifyVirtualViewStructure(MAIN_TITLE, MAIN_FRAME_RECT, 0);

        // Verifies the child frame.
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .createContentCaptureSession(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(0),
                        CHILD_URL,
                        null);

        // Verifies the ViewStructure for the child frame.
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .newVirtualViewStructure(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(0),
                        mPlatformAPIWrapperTestHelper.mCreatedViewStructuresAutofilIds.get(0),
                        CHILD_FRAME_ID);
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .notifyViewAppeared(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(0),
                        mPlatformAPIWrapperTestHelper.mCreatedViewStructures.get(1));
        verifyVirtualViewStructure(null, CHILD_FRAME_RECT, 1);

        // Verifies child1
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .newVirtualViewStructure(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(1),
                        mPlatformAPIWrapperTestHelper.mCreatedViewStructuresAutofilIds.get(1),
                        CHILD1_ID);
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .notifyViewAppeared(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(1),
                        mPlatformAPIWrapperTestHelper.mCreatedViewStructures.get(2));
        verifyVirtualViewStructure(CHILD1_TEXT, CHILD1_RECT, 2);

        // Verifies child2
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .newVirtualViewStructure(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(1),
                        mPlatformAPIWrapperTestHelper.mCreatedViewStructuresAutofilIds.get(1),
                        CHILD2_ID);
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .notifyViewAppeared(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(1),
                        mPlatformAPIWrapperTestHelper.mCreatedViewStructures.get(3));
        verifyVirtualViewStructure(CHILD2_TEXT, CHILD2_RECT, 3);

        // Modifies child2
        runTaskAndVerifyCallback(
                createChild2ContentUpdateTask(),
                toIntArray(
                        PlatformAPIWrapperTestHelper.NEW_AUTOFILL_ID,
                        PlatformAPIWrapperTestHelper.NOTIFY_VIEW_TEXT_CHANGED));
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .notifyViewTextChanged(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(1),
                        mPlatformAPIWrapperTestHelper.mCreatedAutofilIds.get(0),
                        CHILD2_NEW_TEXT);

        // Removes the child1 and child2
        runTaskAndVerifyCallback(
                createRemoveChildrenTask(),
                toIntArray(PlatformAPIWrapperTestHelper.NOTIFY_VIEWS_DISAPPEARED));
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .notifyViewsDisappeared(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(1),
                        mMockedRootAutofillId,
                        REMOVED_IDS);

        // Remove the child frame
        runTaskAndVerifyCallback(
                createSessionRemovedTask(),
                toIntArray(
                        PlatformAPIWrapperTestHelper.DESTROY_CONTENT_CAPTURE_SESSION,
                        PlatformAPIWrapperTestHelper.NOTIFY_VIEW_DISAPPEARED));
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .destroyContentCaptureSession(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(1));

        // Updates the title.
        runTaskAndVerifyCallback(
                createTitleUpdateTask(),
                toIntArray(
                        PlatformAPIWrapperTestHelper.NEW_AUTOFILL_ID,
                        PlatformAPIWrapperTestHelper.NOTIFY_VIEW_TEXT_CHANGED));
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .notifyViewTextChanged(
                        mMockedRootContentCaptureSession,
                        mPlatformAPIWrapperTestHelper.mCreatedAutofilIds.get(1),
                        UPDATED_MAIN_TITLE);

        // Update the favicon
        runTaskAndVerifyCallback(
                createFaviconUpdateTask(),
                toIntArray(PlatformAPIWrapperTestHelper.NOTIFY_FAVICON_UPDATE));
        inOrder.verify(mPlatformAPIWrapperTestHelperSpy)
                .notifyFaviconUpdated(
                        mPlatformAPIWrapperTestHelper.mCreatedContentCaptureSessions.get(0),
                        UPDATED_FAVICON);
    }

    // The below testFooException() tests mock the specific method to throw exception, then verify
    // if it is caught by NotificationTask.
    @Test
    public void testCreateContentCaptureSessionException() throws Throwable {
        PlatformAPIWrapperTestHelper mockedApiWrapperTestHelper =
                Mockito.spy(mPlatformAPIWrapperTestHelper);
        PlatformAPIWrapper.setPlatformAPIWrapperImplForTesting(mockedApiWrapperTestHelper);
        doThrow(createMainContentCaptureSessionException())
                .when(mockedApiWrapperTestHelper)
                .createContentCaptureSession(
                        ArgumentMatchers.any(), ArgumentMatchers.any(), ArgumentMatchers.any());
        runTaskAndVerifyCallbackWithException(createContentCapturedTask(), toIntArray());
    }

    @Test
    public void testNewVirtualViewStructureException() throws Throwable {
        PlatformAPIWrapperTestHelper mockedApiWrapperTestHelper =
                Mockito.spy(mPlatformAPIWrapperTestHelper);
        PlatformAPIWrapper.setPlatformAPIWrapperImplForTesting(mockedApiWrapperTestHelper);
        doThrow(createMainContentCaptureSessionException())
                .when(mockedApiWrapperTestHelper)
                .newVirtualViewStructure(
                        ArgumentMatchers.any(), ArgumentMatchers.any(), ArgumentMatchers.anyLong());
        runTaskAndVerifyCallbackWithException(
                createContentCapturedTask(),
                toIntArray(PlatformAPIWrapperTestHelper.CREATE_CONTENT_CAPTURE_SESSION));
    }

    @Test
    public void testNotifyViewAppearException() throws Throwable {
        PlatformAPIWrapperTestHelper mockedApiWrapperTestHelper =
                Mockito.spy(mPlatformAPIWrapperTestHelper);
        PlatformAPIWrapper.setPlatformAPIWrapperImplForTesting(mockedApiWrapperTestHelper);
        doThrow(createMainContentCaptureSessionException())
                .when(mockedApiWrapperTestHelper)
                .notifyViewAppeared(ArgumentMatchers.any(), ArgumentMatchers.any());
        runTaskAndVerifyCallbackWithException(
                createContentCapturedTask(),
                toIntArray(
                        PlatformAPIWrapperTestHelper.CREATE_CONTENT_CAPTURE_SESSION,
                        PlatformAPIWrapperTestHelper.NEW_VIRTUAL_VIEW_STRUCTURE));
    }

    @Test
    public void testNotifyViewTextChangedException() throws Throwable {
        PlatformAPIWrapperTestHelper mockedApiWrapperTestHelper =
                Mockito.spy(mPlatformAPIWrapperTestHelper);
        PlatformAPIWrapper.setPlatformAPIWrapperImplForTesting(mockedApiWrapperTestHelper);
        doThrow(createMainContentCaptureSessionException())
                .when(mockedApiWrapperTestHelper)
                .notifyViewTextChanged(
                        ArgumentMatchers.any(), ArgumentMatchers.any(), ArgumentMatchers.any());
        runContentCapturedTask();
        // Modifies child2
        runTaskAndVerifyCallbackWithException(
                createChild2ContentUpdateTask(),
                toIntArray(PlatformAPIWrapperTestHelper.NEW_AUTOFILL_ID));
    }

    @Test
    public void testNotifyViewsDisappearedException() throws Throwable {
        PlatformAPIWrapperTestHelper mockedApiWrapperTestHelper =
                Mockito.spy(mPlatformAPIWrapperTestHelper);
        PlatformAPIWrapper.setPlatformAPIWrapperImplForTesting(mockedApiWrapperTestHelper);
        doThrow(createMainContentCaptureSessionException())
                .when(mockedApiWrapperTestHelper)
                .notifyViewsDisappeared(
                        ArgumentMatchers.any(), ArgumentMatchers.any(), ArgumentMatchers.any());
        runContentCapturedTask();
        // Modifies child2
        runTaskAndVerifyCallbackWithException(createRemoveChildrenTask(), toIntArray());
    }

    @Test
    public void testDestroyContentCaptureSessionException() throws Throwable {
        PlatformAPIWrapperTestHelper mockedApiWrapperTestHelper =
                Mockito.spy(mPlatformAPIWrapperTestHelper);
        PlatformAPIWrapper.setPlatformAPIWrapperImplForTesting(mockedApiWrapperTestHelper);
        doThrow(createMainContentCaptureSessionException())
                .when(mockedApiWrapperTestHelper)
                .destroyContentCaptureSession(ArgumentMatchers.any());
        runContentCapturedTask();
        // Modifies child2
        runTaskAndVerifyCallbackWithException(createSessionRemovedTask(), toIntArray());
    }

    @Test
    public void testNotifyViewDisappearedException() throws Throwable {
        PlatformAPIWrapperTestHelper mockedApiWrapperTestHelper =
                Mockito.spy(mPlatformAPIWrapperTestHelper);
        PlatformAPIWrapper.setPlatformAPIWrapperImplForTesting(mockedApiWrapperTestHelper);
        doThrow(createMainContentCaptureSessionException())
                .when(mockedApiWrapperTestHelper)
                .notifyViewDisappeared(ArgumentMatchers.any(), ArgumentMatchers.any());
        runContentCapturedTask();
        // Modifies child2
        runTaskAndVerifyCallbackWithException(
                createSessionRemovedTask(),
                toIntArray(PlatformAPIWrapperTestHelper.DESTROY_CONTENT_CAPTURE_SESSION));
    }
}
