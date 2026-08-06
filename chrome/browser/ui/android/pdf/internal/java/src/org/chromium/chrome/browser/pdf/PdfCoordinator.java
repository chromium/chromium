// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.OpenableColumns;
import android.text.format.Formatter;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;
import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.pdf.PdfDocument;
import androidx.pdf.PdfDocument.PageInfo;
import androidx.pdf.PdfPoint;
import androidx.pdf.PdfSandboxHandle;
import androidx.pdf.PdfWriteHandle;
import androidx.pdf.SandboxedPdfLoader;
import androidx.pdf.content.ExternalLink;
import androidx.pdf.ink.EditablePdfViewerFragment;
import androidx.pdf.view.PdfView;

import kotlin.coroutines.Continuation;
import kotlin.coroutines.CoroutineContext;
import kotlin.coroutines.EmptyCoroutineContext;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.BundleUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfHyperlinkClickResult;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfLoadResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.modules.on_demand.OnDemandModule;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogType;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.io.File;
import java.io.IOException;
import java.lang.ref.WeakReference;
import java.text.DateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.Set;
import java.util.function.Consumer;

/**
 * The class responsible for setting up PdfPage.
 *
 * <p>Lint suppression for NewApi is added because we are using EditablePdfViewerFragment and inline
 * pdf support is enabled via PdfUtils#shouldOpenPdfInline.
 */
@SuppressLint("NewApi")
@NullMarked
public class PdfCoordinator
        implements PdfCoordinatorInterface, PdfActionsDelegate, PdfToolbarActionsDelegate {
    private static final String TAG = "PdfCoordinator";
    private static final String ACTION_ANNOTATE = "android.intent.action.ANNOTATE";
    private static final int PAGE_TRANSITION_TYPE = PageTransition.LINK;

    // PDF link annotations are untrusted input (ISO 32000-1 §12.6.4.7 leaves scheme policy
    // to the viewer). Restrict to schemes that have a meaningful web-navigation or
    // communication semantic, mirroring the defaults used by pdf.js
    // (PDFLinkService.getAnchorUrl) and Adobe Reader's Trust Manager. Blocks dangerous
    // schemes such as javascript:, data:, file:, content:, intent:, chrome:, devtools:.
    private static final Set<String> ALLOWED_LINK_SCHEMES =
            Set.of("http", "https", "mailto", "tel", "ftp");
    private static final float POINTS_PER_INCH = 72.0f;
    private static final float MM_PER_INCH = 25.4f;

    static final String JSON_KEY_FILE_METADATA = "file_metadata";
    static final String JSON_KEY_FILE_URI = "file_uri";
    static final String JSON_KEY_MIME_TYPE = "mime_type";
    static final String JSON_KEY_FILE_NAME = "file_name";
    static final String JSON_KEY_IS_WORK_PROFILE = "is_work_profile";

    /**
     * The timestamp when the last pdf document starts to load. Used to calculate the elapsed time
     * between two pdf loads.
     */
    private static long sLastPdfLoadTimestamp;

    private static boolean sSkipLoadPdfForTesting;

    private final Activity mActivity;
    private final FragmentManager mFragmentManager;
    private final View mView;

    /** ProgressBar to be shown during PDF download. */
    private final ProgressBar mProgressBar;

    private final NativePageHost mNativePageHost;

    private final String mTabId;
    private String mTitle;
    private final String mUrl;
    private final boolean mIsIncognito;

    /** A unique id to identity the FragmentContainerView in the current PdfPage. */
    final int mFragmentContainerViewId;

    @SuppressWarnings("UnusedVariable")
    private @Nullable PdfSelectionCoordinator mPdfSelectionCoordinator;

    private final @Nullable PdfToolbarCoordinator mToolbarCoordinator;

    private final PdfFragmentViewTracker mPdfFragmentViewTracker;

    /** The filepath of the pdf. It is null before download complete. */
    private @Nullable String mPdfFilePath;

    /** Uri of the pdf document. Generated when the pdf is ready to load. */
    private @Nullable Uri mUri;

    /** A PdfSandboxHandle representing the active pdf session. */
    private @Nullable PdfSandboxHandle mPdfSandboxHandle;

    /**
     * Whether the pdf has been loaded, despite of success or failure, for the current mUri. This is
     * used to ensure we load the pdf at most once. If mUri was updated, this is reset to false.
     */
    private boolean mIsPdfLoaded;

    boolean mIsInitialZoomPass = true;

    private int mFindInPageCount;

    private boolean mPageNavAndEditVisible = true;

    @VisibleForTesting public ChromePdfViewerFragment mChromePdfViewerFragment;

    /**
     * Creates a PdfCoordinator for the PdfPage.
     *
     * @param host A NativePageHost to load urls.
     * @param profile The current Profile.
     * @param activity The current Activity.
     * @param filepath The pdf filepath.
     * @param title The pdf title.
     * @param tabId The id of the tab.
     * @param url The url of the pdf.
     */
    public PdfCoordinator(
            NativePageHost host,
            Profile profile,
            Activity activity,
            @Nullable String filepath,
            String title,
            int tabId,
            String url,
            PdfFragmentViewTracker pdfFragmentViewTracker) {
        mActivity = activity;
        mTabId = String.valueOf(tabId);
        mNativePageHost = host;
        mIsIncognito = profile.isOffTheRecord();
        mTitle = title;
        mUrl = url;
        Context contextForInflation =
                BundleUtils.createContextForInflation(activity, OnDemandModule.SPLIT_NAME);
        mView = LayoutInflater.from(contextForInflation).inflate(R.layout.pdf_page, null);
        mPdfFragmentViewTracker = pdfFragmentViewTracker;
        mProgressBar = mView.findViewById(R.id.progress_bar);
        mView.setBackgroundColor(
                ChromeColors.getPrimaryBackgroundColor(activity, profile.isOffTheRecord()));
        mView.addOnAttachStateChangeListener(
                new View.OnAttachStateChangeListener() {
                    @Override
                    public void onViewAttachedToWindow(View view) {
                        // Post to avoid modifying view hierarchy during attachment traversal.
                        view.post(() -> loadPdfFile());
                    }

                    @Override
                    public void onViewDetachedFromWindow(View view) {}
                });
        boolean reuseFragment = PdfUtils.isReuseFragmentEnabled();
        if (reuseFragment) {
            relocateMisplacedFragmentViews();
            mFragmentContainerViewId = R.id.pdf_fragment_container;
        } else {
            View fragmentContainerView = mView.findViewById(R.id.pdf_fragment_container);
            mFragmentContainerViewId = View.generateViewId();
            fragmentContainerView.setId(mFragmentContainerViewId);
        }
        mFragmentManager = ((FragmentActivity) activity).getSupportFragmentManager();
        Fragment fragment = mFragmentManager.findFragmentByTag(mTabId);
        if (fragment != null) {
            if (reuseFragment) {
                mChromePdfViewerFragment = (ChromePdfViewerFragment) fragment;
                mChromePdfViewerFragment.setPagesPerRow(false);
                if (mPdfFilePath == null) {
                    mPdfFilePath =
                            filepath != null ? filepath : mChromePdfViewerFragment.getFilePath();
                }
                String restoredFileName = mChromePdfViewerFragment.getFileName();
                if (mTitle == null && restoredFileName != null) mTitle = restoredFileName;
                if (mUri == null && mPdfFilePath != null) {
                    mUri = PdfUtils.getContentUri(mPdfFilePath, mTitle, mTabId, mIsIncognito);
                }
            } else {
                mFragmentManager.beginTransaction().remove(fragment).commitAllowingStateLoss();
            }
        }

        if (mChromePdfViewerFragment == null) {
            mChromePdfViewerFragment = new ChromePdfViewerFragment(this);
            mChromePdfViewerFragment.setViewTag(mTabId);
            // Start pdf library initialization. This prepares pdf resources ahead of time, so that
            // pdf could be loaded faster when documentUri is set.
            mPdfSandboxHandle = SandboxedPdfLoader.startInitialization(activity);
            // PDF is downloading when the filepath is null.
            if (filepath == null) {
                mProgressBar.setVisibility(View.VISIBLE);
            }
            loadPdfFile(filepath);
        }
        if (PdfUtils.isInlinePdfV2Enabled()) {
            // Hide until zoom stabilizes at 100%.
            mView.findViewById(mFragmentContainerViewId).setVisibility(View.INVISIBLE);
            mToolbarCoordinator = new PdfToolbarCoordinator(mView, this);
        } else {
            mToolbarCoordinator = null;
        }

        if (reuseFragment && fragment != null) {
            mChromePdfViewerFragment.setDelegate(this);
        }
    }

    private void relocateMisplacedFragmentViews() {
        ViewGroup container = mView.findViewById(R.id.pdf_fragment_container);
        if (container.getChildCount() > 0) {
            mPdfFragmentViewTracker.maybeRelocateViews(container, mTabId);
        } else {
            container.setOnHierarchyChangeListener(
                    new ViewGroup.OnHierarchyChangeListener() {
                        @Override
                        public void onChildViewAdded(View parent, View child) {
                            mPdfFragmentViewTracker.maybeRelocateViews(container, mTabId);
                        }

                        @Override
                        public void onChildViewRemoved(View parent, View child) {}
                    });
        }
    }

    /** The class responsible for rendering pdf document. */
    public static class ChromePdfViewerFragment extends EditablePdfViewerFragment {

        static final String KEY_VIEW_TAG = "view_tag";
        static final String KEY_SAVED_PAGE_INDEX = "saved_page_index";
        static final String KEY_SAVED_ZOOM = "saved_zoom";
        static final String KEY_RESTORE_POSITION_PENDING = "restore_position_pending";
        private static final String KEY_FILE_PATH = "file_path";
        private static final String KEY_FILE_NAME = "file_name";
        private @Nullable PdfActionsDelegate mDelegate;
        @VisibleForTesting @Nullable PdfView mPdfView;
        @VisibleForTesting boolean mIsPdfViewSetup;

        @Nullable private String mViewTag;
        private int mSavedPageIndex = -1;
        private float mSavedZoom = -1f;
        private boolean mRestorePositionPending;
        private @Nullable View mToolBoxView;
        private @Nullable ViewGroup mContainerView;
        private int mOriginalIndex;
        private boolean mShowToolBoxView = true;
        @Nullable private String mFilePath;
        @Nullable private String mFileName;

        public void setPdfViewForTesting(PdfView pdfView) {
            this.mPdfView = pdfView;
            mIsPdfViewSetup = false;
            maybeSetupPdfView();
        }

        @Override
        public void onPdfViewCreated(PdfView pdfView) {
            super.onPdfViewCreated(pdfView);
            mPdfView = pdfView;
            mIsPdfViewSetup = false;

            if (getView() != null && mViewTag != null) getView().setTag(mViewTag);
            if (PdfUtils.isInlinePdfV2Enabled()) {
                pdfView.setFormFillingEnabled(
                        PdfUtils.isInlinePdfV2FormFillingEnabled() && !isEditModeEnabled());
            }
            maybeSetupPdfView();
        }

        private void maybeSetupPdfView() {
            if (!PdfUtils.isInlinePdfV2Enabled()
                    || mDelegate == null
                    || mPdfView == null
                    || mIsPdfViewSetup) {
                return;
            }
            mIsPdfViewSetup = true;
            // TODO(crbug.com/498644542): call getPageCount() within onLoadDocumentSuccess()
            mDelegate.loadPdfSelectionCoordinator(mPdfView);
            final PdfView capturedView = mPdfView;
            final PdfActionsDelegate delegate = mDelegate;

            // When the delegate is attached after the fragment was restored by FragmentManager,
            // the PDF document may already be loaded. Trigger the callbacks immediately if so.
            if (capturedView.getPdfDocument() != null) {
                try {
                    delegate.onDocumentLoaded(capturedView.getPdfDocument().getPageCount());
                } catch (PdfDocument.DocumentClosedException e) {
                    Log.w(TAG, "Failed to get page count", e);
                }
                delegate.onViewportChanged(
                        capturedView.getFirstVisiblePage(), capturedView.getZoom());
            } else {
                // Add a one-time listener to track total page count and remove itself afterwards.
                // This listener is necessary because getPdfDocument() can return null up until the
                // viewport is changed.
                capturedView.addOnViewportChangedListener(
                        new PdfView.OnViewportChangedListener() {
                            @Override
                            public void onViewportChanged(
                                    int firstVisiblePage,
                                    int visiblePagesCount,
                                    SparseArray pageLocations,
                                    float zoomLevel) {
                                if (capturedView.getPdfDocument() != null) {
                                    // Post to the UI thread to avoid removing the listener while
                                    // androidx.pdf.view.PdfView is notifying its listeners, which
                                    // can throw an IndexOutOfBoundsException error.
                                    ThreadUtils.postOnUiThread(
                                            () ->
                                                    capturedView.removeOnViewportChangedListener(
                                                            this));
                                    try {
                                        delegate.onDocumentLoaded(
                                                capturedView.getPdfDocument().getPageCount());
                                    } catch (PdfDocument.DocumentClosedException e) {
                                        Log.w(TAG, "Failed to get page count", e);
                                    }
                                }
                            }
                        });
            }

            // Add a persistent listener to track page changes.
            capturedView.addOnViewportChangedListener(
                    (firstVisiblePage, visiblePagesCount, pageLocations, zoomLevel) ->
                            delegate.onViewportChanged(firstVisiblePage, zoomLevel));
        }

        /** Public no-arg constructor for FragmentManager. */
        public ChromePdfViewerFragment() {}

        public ChromePdfViewerFragment(PdfActionsDelegate handler) {
            mDelegate = handler;
        }

        public @Nullable PdfActionsDelegate getDelegate() {
            return mDelegate;
        }

        public void setDelegate(PdfActionsDelegate delegate) {
            if (mDelegate != delegate) {
                mDelegate = delegate;
                maybeSetupPdfView();
            }
        }

        /** Whether the pdf has been loaded successfully. */
        @VisibleForTesting public boolean mIsLoadDocumentSuccess;

        /** Whether the pdf has emitted any load error. */
        boolean mIsLoadDocumentError;

        /** The timestamp when the pdf document starts to load. */
        long mDocumentLoadStartTimestamp;

        public void setViewTag(String tag) {
            mViewTag = tag;
        }

        public void setFilePath(@Nullable String filePath) {
            mFilePath = filePath;
        }

        public @Nullable String getFilePath() {
            return mFilePath;
        }

        public void setFileName(@Nullable String fileName) {
            mFileName = fileName;
        }

        public @Nullable String getFileName() {
            return mFileName;
        }

        @Override
        public void onAttach(Context context) {
            ClassLoader classLoader = ChromePdfViewerFragment.class.getClassLoader();
            Bundle arguments = getArguments();
            if (arguments != null) {
                arguments.setClassLoader(classLoader);
            }
            super.onAttach(context);
        }

        @Override
        public void onCreate(@Nullable Bundle savedInstanceState) {
            if (savedInstanceState != null) {
                savedInstanceState.setClassLoader(ChromePdfViewerFragment.class.getClassLoader());
            }
            super.onCreate(savedInstanceState);
        }

        @Override
        public void onViewCreated(View view, @Nullable Bundle savedInstanceState) {
            Bundle state = savedInstanceState;
            if (state == null) {
                state = getArguments();
            }
            if (state != null) {
                state.setClassLoader(ChromePdfViewerFragment.class.getClassLoader());
                if (state.containsKey(KEY_VIEW_TAG)) {
                    mViewTag = state.getString(KEY_VIEW_TAG, null);
                }
                mSavedPageIndex = state.getInt(KEY_SAVED_PAGE_INDEX, -1);
                mSavedZoom = state.getFloat(KEY_SAVED_ZOOM, -1f);
                mRestorePositionPending = state.getBoolean(KEY_RESTORE_POSITION_PENDING, false);
            }
            super.onViewCreated(view, savedInstanceState);
            if (savedInstanceState != null) {
                if (getView() != null) getView().setTag(mViewTag);
                mFilePath = savedInstanceState.getString(KEY_FILE_PATH, null);
                mFileName = savedInstanceState.getString(KEY_FILE_NAME, null);
            }
            setUpToolBoxView(view);
        }

        @Override
        public void onDestroyView() {
            super.onDestroyView();
            mIsPdfViewSetup = false;
            mPdfView = null;
        }

        @VisibleForTesting
        void setUpToolBoxView(View view) {
            mToolBoxView = view.findViewById(R.id.toolBoxView);
            mContainerView = (ViewGroup) view;
            if (mContainerView != null && mToolBoxView != null) {
                mOriginalIndex = mContainerView.indexOfChild(mToolBoxView);
            }
            if (PdfUtils.isInlinePdfV2Enabled()) {
                if (mDelegate != null) {
                    setToolBoxViewVisibility(!mDelegate.isPageNavAndEditVisible());
                } else {
                    updateToolBoxView();
                }
            } else {
                if (mToolBoxView != null) {
                    overrideClickListeners(mToolBoxView);
                }
            }
        }

        private static final String EXTRA_PDF_FILE_NAME =
                "androidx.pdf.viewer.fragment.extra.PDF_FILE_NAME";
        private static final String EXTRA_STARTING_PAGE =
                "androidx.pdf.viewer.fragment.extra.STARTING_PAGE";

        private void openPdfInExternalEditor() {
            Context context = getContext();
            if (context == null) return;

            Uri uri = getDocumentUri();
            if (uri == null && mDelegate != null) {
                uri = mDelegate.getUri();
            }
            if (uri == null) {
                return;
            }
            if (!PdfUtils.isUriSafeForSharing(uri, context)) {
                Log.e(TAG, "Blocked openPdfInExternalEditor for unsafe URI: " + uri);
                showCannotEditToast(context);
                return;
            }

            if (!resolveAnnotationIntent(context, uri)) {
                hideToolBox();
                showCannotEditToast(context);
                return;
            }

            Intent intent = createAnnotationIntent(uri, context);

            try {
                context.startActivity(intent);
            } catch (ActivityNotFoundException | SecurityException e) {
                Log.w(TAG, "Failed to start PDF annotator activity.", e);
                hideToolBox();
                showCannotEditToast(context);
            }
        }

        private boolean resolveAnnotationIntent(Context context, Uri uri) {
            Intent intent = createAnnotationIntent(uri, context, /* includeExtras= */ false);
            return intent.resolveActivity(context.getPackageManager()) != null;
        }

        private Intent createAnnotationIntent(Uri uri, Context context) {
            return createAnnotationIntent(uri, context, /* includeExtras= */ true);
        }

        private Intent createAnnotationIntent(Uri uri, Context context, boolean includeExtras) {
            Intent intent = new Intent(ACTION_ANNOTATE);
            intent.addCategory(Intent.CATEGORY_DEFAULT);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            intent.setDataAndType(uri, MimeTypeUtils.PDF_MIME_TYPE);

            if (includeExtras) {
                String fileName = getFileName(uri, context.getContentResolver());
                if (fileName != null) {
                    intent.putExtra(EXTRA_PDF_FILE_NAME, fileName);
                }

                int pageNum = 0;
                if (mPdfView != null) {
                    pageNum = mPdfView.getFirstVisiblePage();
                }
                intent.putExtra(EXTRA_STARTING_PAGE, pageNum);
            }

            return intent;
        }

        private @Nullable String getFileName(Uri uri, ContentResolver contentResolver) {
            String fileName = null;
            if (ContentResolver.SCHEME_CONTENT.equals(uri.getScheme())) {
                String[] projection = new String[] {OpenableColumns.DISPLAY_NAME};
                try (android.database.Cursor cursor =
                        contentResolver.query(uri, projection, null, null, null)) {
                    if (cursor != null && cursor.moveToFirst()) {
                        int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                        if (index != -1) {
                            fileName = cursor.getString(index);
                        }
                    }
                } catch (SecurityException
                        | IllegalArgumentException
                        | NullPointerException
                        | IllegalStateException
                        | android.database.SQLException e) {
                    // Ignore
                }
            }
            if (fileName == null) {
                fileName = uri.getLastPathSegment();
            }
            return fileName;
        }

        private void hideToolBox() {
            if (mToolBoxView != null) {
                mToolBoxView.setVisibility(View.GONE);
            }
        }

        private void showCannotEditToast(Context context) {
            Toast.makeText(context, R.string.pdf_cannot_edit_pdf, Toast.LENGTH_SHORT).show();
        }

        public void setToolBoxViewVisibility(boolean visible) {
            if (mShowToolBoxView == visible) {
                return;
            }
            mShowToolBoxView = visible;
            updateToolBoxView();
        }

        private void updateToolBoxView() {
            if (mContainerView == null || mToolBoxView == null) {
                return;
            }
            boolean isCurrentlyAdded = mToolBoxView.getParent() != null;
            if (mShowToolBoxView && !isCurrentlyAdded) {
                int index = Math.min(mOriginalIndex, mContainerView.getChildCount());
                mContainerView.addView(mToolBoxView, index);
            } else if (!mShowToolBoxView && isCurrentlyAdded) {
                mContainerView.removeView(mToolBoxView);
            }
        }

        @Override
        public void onEnterEditMode() {
            super.onEnterEditMode();
            if (mDelegate != null) {
                mDelegate.onEditModeChanged(true);
            }
        }

        @Override
        public void onExitEditMode() {
            super.onExitEditMode();
            if (mDelegate != null) {
                mDelegate.onEditModeChanged(false);
            }
        }

        private void cleanupWriteResources(
                @Nullable ParcelFileDescriptor pfd, @Nullable PdfWriteHandle handle) {
            // Can be null if we failed to open the file descriptor (e.g. invalid URI, null
            // context, or IOException during open), or if we are only cleaning up the handle.
            if (pfd != null) {
                try {
                    pfd.close();
                } catch (IOException e) {
                    Log.e(TAG, "Failed to close ParcelFileDescriptor", e);
                }
            }
            // Can be null if we are only cleaning up the pfd (e.g. in the catch block of
            // onApplyEditsSuccess to avoid double-closing the handle which is closed at the end of
            // the method).
            if (handle != null) {
                try {
                    handle.close();
                } catch (IOException e) {
                    Log.e(TAG, "Failed to close PdfWriteHandle", e);
                }
            }
        }

        @Override
        public void onApplyEditsSuccess(PdfWriteHandle handle) {
            Uri uri = getDocumentUri();

            if (uri != null && getContext() != null) {
                ParcelFileDescriptor pfd = null;
                boolean success = false;
                try {
                    if (ContentResolver.SCHEME_CONTENT.equals(uri.getScheme())) {
                        pfd = getContext().getContentResolver().openFileDescriptor(uri, "w");
                    } else if (ContentResolver.SCHEME_FILE.equals(uri.getScheme())) {
                        String path = uri.getPath();
                        if (path != null) {
                            pfd =
                                    ParcelFileDescriptor.open(
                                            new File(path),
                                            ParcelFileDescriptor.MODE_WRITE_ONLY
                                                    | ParcelFileDescriptor.MODE_TRUNCATE);
                        } else {
                            Log.e(TAG, "File URI has null path: " + uri);
                        }
                    }

                    if (pfd != null) {
                        final ParcelFileDescriptor finalPfd = pfd;
                        Continuation<kotlin.Unit> continuation =
                                new Continuation<kotlin.Unit>() {
                                    @Override
                                    public CoroutineContext getContext() {
                                        return EmptyCoroutineContext.INSTANCE;
                                    }

                                    @Override
                                    public void resumeWith(Object result) {
                                        if (result != kotlin.Unit.INSTANCE) {
                                            Log.e(TAG, "Async PDF write failed: " + result);
                                        }
                                        PostTask.postTask(
                                                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                                                () -> {
                                                    cleanupWriteResources(finalPfd, handle);
                                                    ThreadUtils.postOnUiThread(() -> finishExitingEditMode());
                                                });
                                    }
                                };

                        if (mPdfView != null) {
                            mSavedPageIndex = mPdfView.getFirstVisiblePage();
                            mSavedZoom = mPdfView.getZoom();
                            mRestorePositionPending = true;
                        }

                        Object coroutineResult = handle.writeTo(pfd, continuation);

                        if (coroutineResult
                                != kotlin.coroutines.intrinsics.IntrinsicsKt
                                        .getCOROUTINE_SUSPENDED()) {
                            // Completed synchronously.
                            PostTask.postTask(
                                    TaskTraits.USER_BLOCKING_MAY_BLOCK,
                                    () -> {
                                        cleanupWriteResources(finalPfd, handle);
                                        ThreadUtils.postOnUiThread(() -> finishExitingEditMode());
                                    });
                        }
                        success = true;
                        return;
                    } else {
                        Log.e(TAG, "Failed to open file descriptor for writing: " + uri);
                    }
                } catch (IOException e) {
                    Log.e(TAG, "Failed to write PDF edits", e);
                } finally {
                    if (!success) {
                        cleanupWriteResources(pfd, handle);
                        setEditModeEnabled(false);
                    }
                }
            } else {
                Log.e(TAG, "Cannot write edits, uri or context is null. Uri: " + uri);
                cleanupWriteResources(null, handle);
                setEditModeEnabled(false);
            }
        }

        private void finishExitingEditMode() {
            setEditModeEnabled(false);
        }

        @Override
        // TODO(crbug.com/527937210): Handle this error in a user-friendly way.
        public void onApplyEditsFailed(Throwable error) {
            Log.e(TAG, "Failed to apply PDF edits", error);
            setEditModeEnabled(false);
        }

        @Override
        public void onSaveInstanceState(Bundle outState) {
            super.onSaveInstanceState(outState);
            outState.putString(KEY_VIEW_TAG, mViewTag);
            outState.putInt(KEY_SAVED_PAGE_INDEX, mSavedPageIndex);
            outState.putFloat(KEY_SAVED_ZOOM, mSavedZoom);
            outState.putBoolean(KEY_RESTORE_POSITION_PENDING, mRestorePositionPending);
            outState.putString(KEY_FILE_PATH, mFilePath);
            outState.putString(KEY_FILE_NAME, mFileName);
        }

        @Override
        public boolean onLinkClicked(ExternalLink externalLink) {
            if (mDelegate == null) {
                return false;
            }
            return mDelegate.onLinkClicked(externalLink.getUri());
        }

        @Override
        public void onLoadDocumentSuccess(PdfDocument pdfDocument) {
            super.onLoadDocumentSuccess(pdfDocument);
            maybeHideToolBoxForUnsupportedEdit();
            if (PdfUtils.isInlinePdfV2Enabled() && mPdfView != null) {
                mPdfView.setFormFillingEnabled(
                        PdfUtils.isInlinePdfV2FormFillingEnabled() && !isEditModeEnabled());
            }
            if (mRestorePositionPending && mPdfView != null) {
                mRestorePositionPending = false;
                if (mSavedZoom > 0) {
                    final float zoom = mSavedZoom;
                    mPdfView.post(() -> zoomTo(zoom));
                }
                if (mSavedPageIndex >= 0) {
                    final int page = mSavedPageIndex;
                    mPdfView.post(() -> scrollToPage(page));
                }
            }
            if (mDocumentLoadStartTimestamp <= 0) {
                return;
            }
            // There should be only one success callback for each pdf. Add this confidence check to
            // be consistent with the error callback.
            if (!mIsLoadDocumentSuccess) {
                PdfUtils.recordPdfLoadTimeFirstPaired(
                        SystemClock.elapsedRealtime() - mDocumentLoadStartTimestamp);
                PdfUtils.recordPdfLoadResultDetail(PdfLoadResult.SUCCESS);
            }
            mIsLoadDocumentSuccess = true;
        }

        private void maybeHideToolBoxForUnsupportedEdit() {
            Context context = getContext();
            if (context == null) return;

            Uri uri = getDocumentUri();
            if (uri == null && mDelegate != null) {
                uri = mDelegate.getUri();
            }
            if (uri == null) {
                return;
            }
            if (!PdfUtils.isUriSafeForSharing(uri, context)
                    || !resolveAnnotationIntent(context, uri)) {
                hideToolBox();
            }
        }

        @Override
        public void onLoadDocumentError(Throwable throwable) {
            if (mDocumentLoadStartTimestamp <= 0) {
                return;
            }
            // Only record the first error emitted.
            if (!mIsLoadDocumentError) {
                PdfUtils.recordPdfLoadResultDetail(PdfLoadResult.ERROR);
            }
            mIsLoadDocumentError = true;
            if (mDelegate != null) {
                mDelegate.onDocumentLoadFailed();
            }
        }

        @VisibleForTesting
        static int getSafePageIndex(int pageIndex, int pageCount) {
            return pageCount > 0
                    ? Math.min(Math.max(0, pageIndex), pageCount - 1)
                    : Math.max(0, pageIndex);
        }

        void scrollToPage(int pageIndex) {
            if (mPdfView != null) {
                PdfDocument pdfDocument = mPdfView.getPdfDocument();
                int pageCount = pdfDocument != null ? pdfDocument.getPageCount() : 0;
                int safePageIndex = getSafePageIndex(pageIndex, pageCount);
                // 1. Get the current height of the view in pixels.
                float viewHeightPx = mPdfView.getHeight();

                // 2. Get the current zoom level.
                float currentZoom = mPdfView.getZoom();

                // 3. Calculate half of the viewport height in PDF content units (points).
                // Formula: (Pixels / 2) / Zoom = Content Points
                // When the viewer "centers" this point, the top of the page (0,0)
                // will be pushed exactly to the top of the screen.
                float yOffsetPoints = (viewHeightPx / 2f) / currentZoom;

                // 4. Use the single-argument scrollToPosition.
                // The internal logic will center this offset, resulting in a top-aligned page.
                mPdfView.scrollToPosition(new PdfPoint(safePageIndex, 0f, yOffsetPoints));
            }
        }

        void setDefaultZoom(int pageIndex) {
            PdfView pdfView = mPdfView;
            if (pdfView == null) return;

            // 1. Get the viewport width in actual screen pixels
            int viewportWidthPx =
                    pdfView.getWidth() - pdfView.getPaddingLeft() - pdfView.getPaddingRight();

            // 2. Convert screen pixels directly to DP using Android's density
            float density = pdfView.getContext().getResources().getDisplayMetrics().density;
            float viewportWidthDp = viewportWidthPx / density;

            runWithPageInfo(
                    pageIndex,
                    pageInfo -> {
                        float newZoom =
                                calculateFitToPageZoom(
                                        pageInfo,
                                        /* fitToPageHeight= */ false,
                                        pdfView,
                                        /* zoomRatio= */ viewportWidthDp >= 600 ? 0.8f : 1.0f);
                        pdfView.post(
                                () -> {
                                    pdfView.setZoom(newZoom);
                                });
                    });
        }

        void setPagesPerRow(boolean twoPagesPerRowEnabled) {
            if (mPdfView != null) {
                mPdfView.setPagesPerRow(twoPagesPerRowEnabled ? 2 : 1);
            }
        }

        void zoomTo(float zoomLevel) {
            if (mPdfView != null) {
                mPdfView.setZoom(zoomLevel);
            }
        }

        @VisibleForTesting
        float calculateFitToPageZoom(
                PageInfo info, boolean fitToPageHeight, PdfView pdfView, float zoomRatio) {
            int contentSize = fitToPageHeight ? info.getHeight() : info.getWidth();
            if (contentSize <= 0) return 0f;

            int viewportSize =
                    fitToPageHeight
                            ? pdfView.getHeight()
                                    - pdfView.getPaddingTop()
                                    - pdfView.getPaddingBottom()
                            : pdfView.getWidth()
                                    - pdfView.getPaddingLeft()
                                    - pdfView.getPaddingRight();
            if (viewportSize <= 0) return 0f;

            float newZoom = ((float) viewportSize * zoomRatio) / contentSize;
            return Math.max(pdfView.getMinZoom(), Math.min(newZoom, pdfView.getMaxZoom()));
        }

        private void runWithPageInfo(int pageIndex, Consumer<PageInfo> action) {
            PdfView pdfView = mPdfView;
            if (pdfView == null) return;

            // pdfDocument can legitimately be null during tab teardown or concurrent switches.
            PdfDocument pdfDocument = pdfView.getPdfDocument();
            if (pdfDocument == null) return;

            int pageCount = pdfDocument.getPageCount();
            int safePageIndex = getSafePageIndex(pageIndex, pageCount);

            try {
                pdfDocument.getPageInfo(
                        safePageIndex,
                        new Continuation<PageInfo>() {
                            @Override
                            public CoroutineContext getContext() {
                                return EmptyCoroutineContext.INSTANCE;
                            }

                            @Override
                            public void resumeWith(Object result) {
                                if (result instanceof PageInfo) {
                                    action.accept((PageInfo) result);
                                } else {
                                    Log.w(TAG, "Failed to get page info. Result: " + result);
                                }
                            }
                        });
            } catch (PdfDocument.DocumentClosedException e) {
                Log.w(TAG, "Failed to get page info", e);
            }
        }

        void fitToPage(boolean fitToPageHeight, int pageIndex) {
            PdfView pdfView = mPdfView;
            if (pdfView == null) return;

            runWithPageInfo(
                    pageIndex,
                    pageInfo -> {
                        float newZoom =
                                calculateFitToPageZoom(
                                        pageInfo, fitToPageHeight, pdfView, /* zoomRatio= */ 1.0f);
                        pdfView.post(
                                () -> {
                                    pdfView.setZoom(newZoom);
                                    // Scroll to the top of the page after zooming.
                                    if (fitToPageHeight) scrollToPage(pageIndex);
                                });
                    });
        }

        @Override
        public void onResume() {
            super.onResume();
            if (!PdfUtils.isInlinePdfV2Enabled() && mToolBoxView != null) {
                overrideClickListeners(mToolBoxView);
            }
        }

        private void overrideClickListeners(View view) {
            view.setOnClickListener(v -> openPdfInExternalEditor());
            if (view instanceof ViewGroup) {
                ViewGroup group = (ViewGroup) view;
                for (int i = 0; i < group.getChildCount(); i++) {
                    overrideClickListeners(group.getChildAt(i));
                }
            }
        }
    }

    /** Returns the intended view for PdfPage tab. */
    @Override
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public View getView() {
        return mView;
    }

    /**
     * Show pdf specific find in page UI.
     *
     * @return whether the pdf specific find in page UI is shown.
     */
    @Override
    public boolean findInPage() {
        if (mChromePdfViewerFragment != null && mChromePdfViewerFragment.mIsLoadDocumentSuccess) {
            mChromePdfViewerFragment.setTextSearchActive(true);
            PdfUtils.recordFindInPage(mFindInPageCount++);
            return true;
        }
        return false;
    }

    /**
     * Called after a pdf page has been removed from the view hierarchy and will no longer be used.
     */
    @Override
    @SuppressWarnings({"NullAway"})
    public void destroy() {
        if (mPdfSandboxHandle != null) {
            mPdfSandboxHandle.close();
            mPdfSandboxHandle = null;
        }
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.destroy();
        }
        if (mChromePdfViewerFragment == null) {
            return;
        }
        mPdfFragmentViewTracker.removeViewWithTag(mTabId);

        // Record abort when there is paired pdf load but no load success or error.
        if (mChromePdfViewerFragment.mDocumentLoadStartTimestamp > 0
                && !mChromePdfViewerFragment.mIsLoadDocumentSuccess
                && !mChromePdfViewerFragment.mIsLoadDocumentError) {
            PdfUtils.recordPdfLoadResultDetail(PdfLoadResult.ABORT);
        }
        if (!mFragmentManager.isDestroyed() && mChromePdfViewerFragment.getDelegate() == this) {
            mFragmentManager
                    .beginTransaction()
                    .remove(mChromePdfViewerFragment)
                    .commitAllowingStateLoss();
        }
        mChromePdfViewerFragment = null;
    }

    /**
     * Called after pdf download complete.
     *
     * @param pdfFilePath The filepath of the downloaded pdf document.
     * @param pdfFileName The filename of the downloaded pdf document.
     */
    @Override
    public void onDownloadComplete(String pdfFilePath, String pdfFileName) {
        mTitle = pdfFileName;
        // `mIsPdfLoaded` is true when the PDF is reloaded. In this case, a new download is
        // triggered while the current PDF is still loaded. Since the `PdfCoordinator` is reused,
        // `mIsPdfLoaded` remains true. We then reload the fragment with the new file path.
        // This reload flow is only used when fragment reuse is disabled.
        if (mIsPdfLoaded) {
            assert !PdfUtils.isReuseFragmentEnabled();
            mPdfFilePath = pdfFilePath;
            mUri = PdfUtils.getContentUri(mPdfFilePath, mTitle, mTabId, mIsIncognito);
            reload();
        } else {
            loadPdfFile(pdfFilePath);
        }
    }

    /** Returns the filepath of the pdf document. */
    @Nullable
    @Override
    public String getFilepath() {
        return mPdfFilePath;
    }

    private void loadPdfFile(@Nullable String pdfFilePath) {
        mPdfFilePath = pdfFilePath;
        loadPdfFile();
    }

    @Override
    public void resetLoadState() {
        mIsPdfLoaded = false;
        if (mChromePdfViewerFragment != null) {
            mChromePdfViewerFragment.setPagesPerRow(false);
        }
        // Reset two-pages-per-row state early so the overflow menu doesn't show a stale label while
        // loading, and to prevent permanent out-of-sync state if loading fails or is aborted before
        // onDocumentLoaded() is invoked.
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.resetTwoPagesPerRow();
        }
    }

    private void loadPdfFile() {
        if (mIsPdfLoaded) {
            return;
        }
        if (mPdfFilePath == null) {
            return;
        }
        if (mView.getParent() == null) {
            return;
        }
        mUri = PdfUtils.getContentUri(mPdfFilePath, mTitle, mTabId, mIsIncognito);
        PdfUtils.recordIsUriNull(mUri == null);
        loadPdfInternal();
    }

    @Override
    public void reload() {
        if (mUri == null) {
            return;
        }
        // Reset two-pages-per-row state early so the overflow menu doesn't show a stale label while
        // reloading, and to prevent permanent out-of-sync state if the reload fails or is aborted
        // before onDocumentLoaded() is invoked.
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.resetTwoPagesPerRow();
        }
        int page = -1;
        float zoom = -1f;
        boolean pending = false;
        if (mChromePdfViewerFragment != null) {
            if (mChromePdfViewerFragment.mRestorePositionPending) {
                page = mChromePdfViewerFragment.mSavedPageIndex;
                zoom = mChromePdfViewerFragment.mSavedZoom;
                pending = true;
            } else if (mChromePdfViewerFragment.mPdfView != null) {
                page = mChromePdfViewerFragment.mPdfView.getFirstVisiblePage();
                zoom = mChromePdfViewerFragment.mPdfView.getZoom();
                pending = true;
            }
        }

        mIsPdfLoaded = false;
        // Remove current fragment.
        mFragmentManager
                .beginTransaction()
                .remove(mChromePdfViewerFragment)
                .commitAllowingStateLoss();
        mFragmentManager.executePendingTransactions();

        // Create new fragment.
        mChromePdfViewerFragment = new ChromePdfViewerFragment(this);

        Bundle args = new Bundle();
        args.putInt(ChromePdfViewerFragment.KEY_SAVED_PAGE_INDEX, page);
        args.putFloat(ChromePdfViewerFragment.KEY_SAVED_ZOOM, zoom);
        args.putBoolean(ChromePdfViewerFragment.KEY_RESTORE_POSITION_PENDING, pending);
        mChromePdfViewerFragment.setArguments(args);

        if (mView.getParent() == null) {
            return;
        }

        // Add new fragment and load document again.
        loadPdfInternal();
    }

    private void loadPdfInternal() {
        if (mUri != null) {
            if (sSkipLoadPdfForTesting) {
                mIsPdfLoaded = true;
            } else {
                if (!mChromePdfViewerFragment.isAdded()) {
                    FragmentTransaction transaction = mFragmentManager.beginTransaction();
                    transaction.add(mFragmentContainerViewId, mChromePdfViewerFragment, mTabId);
                    transaction.commitAllowingStateLoss();
                    mFragmentManager.executePendingTransactions();
                }
                PdfUtils.recordPdfLoad();
                long currentTime = SystemClock.elapsedRealtime();
                mChromePdfViewerFragment.mDocumentLoadStartTimestamp = currentTime;
                if (sLastPdfLoadTimestamp > 0) {
                    PdfUtils.recordPdfLoadInterval(currentTime - sLastPdfLoadTimestamp);
                }
                sLastPdfLoadTimestamp = currentTime;
                mProgressBar.setVisibility(View.GONE);
                try {
                    mIsInitialZoomPass = true;
                    if (!mUri.equals(mChromePdfViewerFragment.getDocumentUri())) {
                        mChromePdfViewerFragment.setDocumentUri(mUri);
                        mChromePdfViewerFragment.setFilePath(mPdfFilePath);
                        mChromePdfViewerFragment.setFileName(mTitle);
                    }
                } catch (IllegalArgumentException e) {
                    Log.e(TAG, "Load pdf fails due to invalid uri.", e);
                } finally {
                    mIsPdfLoaded = true;
                }
            }
        } else {
            // TODO(crbug.com/348712628): show some error UI when content URI is null.
            Log.e(TAG, "Uri is null.");
        }
    }

    /**
     * Returns the URI of the PDF file and grants read permission to the specified target package.
     * If the target package is null, the assistant package is set as the target.
     *
     * @param isWorkProfile Whether the current profile is a work profile.
     * @param targetPackage The package name to grant URI permission to. If null, the default
     *     assistant package is used.
     * @return The URI of the PDF file, or null if the URI is not available.
     */
    @Override
    public @Nullable Uri getFileUri(boolean isWorkProfile, @Nullable String targetPackage) {
        if (mUri == null) {
            return null;
        }

        if (!PdfUtils.isUriSafeForSharing(mUri, mActivity)) {
            Log.e(TAG, "Blocked getFileUri for unsafe URI: " + mUri);
            return null;
        }

        if (targetPackage == null) {
            targetPackage = PackageUtils.getDefaultAssistantPackageName(mActivity);
            PdfUtils.recordGetAssistantPackageResult(targetPackage != null);
        }

        if (targetPackage != null) {
            mActivity.grantUriPermission(
                    targetPackage, mUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        PdfUtils.recordIsWorkProfile(isWorkProfile);
        return mUri;
    }

    @Override
    public @Nullable String requestAssistContent(String filename, boolean isWorkProfile) {
        if (mUri == null) {
            return null;
        }

        if (!PdfUtils.isUriSafeForSharing(mUri, mActivity)) {
            Log.e(TAG, "Blocked requestAssistContent for unsafe URI: " + mUri);
            return null;
        }

        String structuredData;
        try {
            structuredData =
                    new JSONObject()
                            .put(
                                    JSON_KEY_FILE_METADATA,
                                    new JSONObject()
                                            .put(JSON_KEY_FILE_NAME, filename)
                                            .put(JSON_KEY_FILE_URI, mUri.toString())
                                            .put(JSON_KEY_MIME_TYPE, MimeTypeUtils.PDF_MIME_TYPE)
                                            .put(JSON_KEY_IS_WORK_PROFILE, isWorkProfile))
                            .toString();
        } catch (JSONException e) {
            return null;
        }
        var assistantPackageName = PackageUtils.getDefaultAssistantPackageName(mActivity);
        PdfUtils.recordGetAssistantPackageResult(assistantPackageName != null);
        if (assistantPackageName != null) {
            mActivity.grantUriPermission(
                    assistantPackageName, mUri, Intent.FLAG_GRANT_READ_URI_PERMISSION);
        }
        PdfUtils.recordIsWorkProfile(isWorkProfile);
        return structuredData;
    }

    @Override
    public @Nullable Uri getUri() {
        return mUri;
    }

    boolean getIsPdfLoadedForTesting() {
        return mIsPdfLoaded;
    }

    static void skipLoadPdfForTesting(boolean skipLoadPdfForTesting) {
        var oldValue = sSkipLoadPdfForTesting;
        sSkipLoadPdfForTesting = skipLoadPdfForTesting;
        ResettersForTesting.register(() -> sSkipLoadPdfForTesting = oldValue);
    }

    // Implementation of PdfToolbarActionsDelegate
    /**
     * Navigates to the specified page.
     *
     * @param pageIndex The 0-based index of the page to navigate to.
     */
    @Override
    public void navigateToPage(int pageIndex) {
        mChromePdfViewerFragment.scrollToPage(pageIndex);
    }

    /**
     * Sets the zoom level to a specified amount.
     *
     * @param zoomLevel The new value of the zoom.
     */
    @Override
    public void changeZoomLevel(float zoomLevel) {
        mChromePdfViewerFragment.zoomTo(zoomLevel);
    }

    /**
     * Sets the edit mode of the PDF toolbar.
     *
     * @param editMode Whether to enable edit mode.
     */
    @Override
    public void setEditMode(boolean editMode) {
        if (!editMode && mChromePdfViewerFragment.hasUnsavedChanges()) {
            mChromePdfViewerFragment.applyDraftEdits();
        } else {
            mChromePdfViewerFragment.setEditModeEnabled(editMode);
        }
    }

    /**
     * Toggles between "fit to page height" and "fit to page width" modes.
     *
     * @param fitToPageHeight Whether to fit to page height or fit to page width.
     * @param pageIndex The 0-based index of page to update scaling.
     */
    @Override
    public void toggleFitToPage(boolean fitToPageHeight, int pageIndex) {
        mChromePdfViewerFragment.fitToPage(fitToPageHeight, pageIndex);
    }

    @Override
    public void toggleTwoPagesPerRow(
            boolean twoPagesPerRowEnabled, float zoomLevel, int currentPageIndex) {
        assert mToolbarCoordinator != null;
        mChromePdfViewerFragment.setPagesPerRow(twoPagesPerRowEnabled);
        mChromePdfViewerFragment.zoomTo(zoomLevel);
        mChromePdfViewerFragment.scrollToPage(currentPageIndex);
    }

    @Override
    public void download() {
        // TODO(crbug.com/501138999): Implement download action
    }

    @Override
    public void print() {
        mNativePageHost.print();
    }

    @Override
    public void onPageNavAndEditVisibilityChanged(boolean visible) {
        mPageNavAndEditVisible = visible;
        if (mChromePdfViewerFragment != null) {
            mChromePdfViewerFragment.setToolBoxViewVisibility(!visible);
        }
    }

    // Implementation of PdfActionsDelegate

    @Override
    public void loadPdfSelectionCoordinator(PdfView pdfView) {
        mPdfSelectionCoordinator = new PdfSelectionCoordinator(mActivity, pdfView);
        if (mToolbarCoordinator != null) {
            pdfView.setFocusable(true);
            pdfView.setFocusableInTouchMode(true);
            pdfView.setOnKeyListener(mToolbarCoordinator);
        }
    }

    @Override
    public boolean onLinkClicked(Uri uri) {
        if (!PdfUtils.isInlinePdfV2Enabled()) {
            PdfUtils.recordHyperlinkClickResult(PdfHyperlinkClickResult.IGNORED_V2_DISABLED);
            return false;
        }
        String scheme = uri.getScheme();
        if (scheme == null || !ALLOWED_LINK_SCHEMES.contains(scheme.toLowerCase(Locale.ROOT))) {
            PdfUtils.recordHyperlinkClickResult(PdfHyperlinkClickResult.BLOCKED_INVALID_SCHEME);
            return false;
        }
        LoadUrlParams params = new LoadUrlParams(uri.toString(), PAGE_TRANSITION_TYPE);
        params.setIsRendererInitiated(true);
        // TODO(crbug.com/484103003): Reconsider initiator origin if renderer initiated is true.
        params.setInitiatorOrigin(Origin.create(new GURL(mUrl)));
        mNativePageHost.loadUrl(params, mIsIncognito);
        PdfUtils.recordHyperlinkClickResult(PdfHyperlinkClickResult.SUCCESS_LOAD_INITIATED);
        return true;
    }

    @Override
    public void onDocumentLoaded(int pageCount) {
        assert mToolbarCoordinator != null;
        assert mUri != null;
        assert mTitle != null;
        mToolbarCoordinator.onDocumentLoaded(pageCount, mTitle);
    }

    @Override
    public void onDocumentLoadFailed() {
        if (PdfUtils.isInlinePdfV2Enabled()) {
            View fragmentContainerView = mView.findViewById(mFragmentContainerViewId);
            if (fragmentContainerView != null
                    && fragmentContainerView.getVisibility() != View.VISIBLE) {
                fragmentContainerView.setVisibility(View.VISIBLE);
            }
        }
    }

    @Override
    public void onEditModeChanged(boolean editMode) {
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.setEditModeActive(editMode);
        }
    }

    @Override
    public boolean isPageNavAndEditVisible() {
        return mPageNavAndEditVisible;
    }

    @Override
    public void onViewportChanged(int pageIndex, float zoomLevel) {
        assert mToolbarCoordinator != null;
        // AndroidX PDF Viewport is not initialized to 100% zoom on the initial pass. For PDF V2, we
        // hide the view until the first viewport change is detected and set to 100% zoom.
        if (PdfUtils.isInlinePdfV2Enabled() && mIsInitialZoomPass) {
            ChromePdfViewerFragment fragment = mChromePdfViewerFragment;
            if (fragment != null) {
                PdfView pdfView = fragment.mPdfView;
                if (pdfView != null && pdfView.getPdfDocument() != null && pdfView.getWidth() > 0) {
                    fragment.setDefaultZoom(pageIndex);
                    mIsInitialZoomPass = false;
                    View fragmentContainerView = mView.findViewById(mFragmentContainerViewId);
                    if (fragmentContainerView != null
                            && fragmentContainerView.getVisibility() != View.VISIBLE) {
                        fragmentContainerView.setVisibility(View.VISIBLE);
                        pdfView.requestFocus();
                    }
                    return;
                }
            }
        }
        mToolbarCoordinator.onViewportChanged(pageIndex, zoomLevel);
    }



    private String formatPageSize(PageInfo pageInfo) {
        float widthInches = pageInfo.getWidth() / POINTS_PER_INCH;
        float heightInches = pageInfo.getHeight() / POINTS_PER_INCH;
        int widthMm = Math.round(widthInches * MM_PER_INCH);
        int heightMm = Math.round(heightInches * MM_PER_INCH);

        return String.format(
                Locale.getDefault(),
                "%.2f × %.2f in (%d × %d mm)",
                widthInches,
                heightInches,
                widthMm,
                heightMm);
    }

    private String formatFileSize(long bytes) {
        return Formatter.formatFileSize(mActivity, bytes);
    }

    private String formatTimestamp(long timestamp) {
        if (timestamp <= 0) {
            return mActivity.getString(R.string.pdf_properties_value_unknown);
        }
        return DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT)
                .format(new Date(timestamp));
    }

    @Override
    public void showDocumentProperties() {
        if (mChromePdfViewerFragment == null) return;
        PdfView pdfView = mChromePdfViewerFragment.mPdfView;
        // pdfDocument can legitimately be null during tab teardown or concurrent switches.
        if (pdfView == null || pdfView.getPdfDocument() == null) return;

        Context appContext = mActivity.getApplicationContext();
        Uri uri = mUri;
        String title = mTitle;
        String pdfFilePath = mPdfFilePath;
        WeakReference<PdfCoordinator> weakSelf = new WeakReference<>(this);

        mChromePdfViewerFragment.runWithPageInfo(
                0,
                pageInfo -> {
                    // Fetch properties on a background thread to avoid UI thread block
                    PostTask.postTask(
                            TaskTraits.USER_VISIBLE,
                            () -> {
                                PdfDocumentPropertiesFetcher.DocProperties fileProps =
                                        PdfDocumentPropertiesFetcher.getDocProperties(
                                                appContext, uri, title, pdfFilePath);
                                // Post back to UI thread to show dialog
                                ThreadUtils.postOnUiThread(
                                        () -> {
                                            PdfCoordinator self = weakSelf.get();
                                            if (self != null) {
                                                self.displayPropertiesDialog(pageInfo, fileProps);
                                            }
                                        });
                            });
                });
    }

    private void displayPropertiesDialog(
            PageInfo pageInfo, PdfDocumentPropertiesFetcher.DocProperties fileProps) {
        if (mActivity == null || mActivity.isFinishing() || mActivity.isDestroyed()) return;
        if (mChromePdfViewerFragment == null) return; // Abort if PdfCoordinator was destroyed

        String fileName = fileProps.mFileName;
        String fileSize = formatFileSize(fileProps.mFileSize);
        String title = mTitle;
        String created = formatTimestamp(fileProps.mCreationTime);
        String modified = formatTimestamp(fileProps.mLastModified);

        int pageCount = 0;
        if (mChromePdfViewerFragment != null
                && mChromePdfViewerFragment.mPdfView != null
                && mChromePdfViewerFragment.mPdfView.getPdfDocument() != null) {
            try {
                pageCount = mChromePdfViewerFragment.mPdfView.getPdfDocument().getPageCount();
            } catch (PdfDocument.DocumentClosedException e) {
                Log.w(TAG, "Failed to get page count for properties dialog", e);
            }
        }
        String pageCountStr = String.valueOf(pageCount);
        String pageSizeStr = formatPageSize(pageInfo);

        View dialogView =
                LayoutInflater.from(mActivity).inflate(R.layout.pdf_properties_dialog, null);

        ((TextView) dialogView.findViewById(R.id.file_name_value)).setText(fileName);
        ((TextView) dialogView.findViewById(R.id.file_size_value)).setText(fileSize);
        ((TextView) dialogView.findViewById(R.id.title_value)).setText(title);
        ((TextView) dialogView.findViewById(R.id.created_value)).setText(created);
        ((TextView) dialogView.findViewById(R.id.modified_value)).setText(modified);
        ((TextView) dialogView.findViewById(R.id.page_count_value)).setText(pageCountStr);
        ((TextView) dialogView.findViewById(R.id.page_size_value)).setText(pageSizeStr);

        if (mActivity instanceof ModalDialogManagerHolder) {
            ModalDialogManager modalDialogManager =
                    ((ModalDialogManagerHolder) mActivity).getModalDialogManager();
            showModalDialog(modalDialogManager, dialogView);
        } else {
            showAlertDialog(dialogView);
        }
    }

    private void showModalDialog(ModalDialogManager manager, View customView) {
        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {}

                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            manager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                        }
                    }
                };

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.CUSTOM_VIEW, customView)
                        .with(ModalDialogProperties.WRAP_CUSTOM_VIEW_IN_SCROLLABLE, true)
                        .with(
                                ModalDialogProperties.TITLE,
                                mActivity.getString(R.string.pdf_document_properties))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mActivity.getResources(),
                                R.string.pdf_properties_close)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NO_NEGATIVE)
                        .build();

        manager.showDialog(model, ModalDialogType.APP);
    }

    private void showAlertDialog(View dialogView) {
        new AlertDialog.Builder(mActivity)
                .setTitle(R.string.pdf_document_properties)
                .setView(dialogView)
                .setPositiveButton(
                        R.string.pdf_properties_close, (dialog, which) -> dialog.dismiss())
                .show();
    }
}
