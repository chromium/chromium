// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.pdf;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.database.SQLException;
import android.graphics.RectF;
import android.net.Uri;
import android.os.Bundle;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.os.SystemClock;
import android.provider.MediaStore.Downloads;
import android.provider.MediaStore.MediaColumns;
import android.provider.OpenableColumns;
import android.system.Os;
import android.text.format.Formatter;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.StringRes;
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

import kotlin.Unit;
import kotlin.coroutines.Continuation;
import kotlin.coroutines.CoroutineContext;
import kotlin.coroutines.EmptyCoroutineContext;
import kotlin.coroutines.intrinsics.IntrinsicsKt;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.BundleUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.PackageUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TriState;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfHyperlinkClickResult;
import org.chromium.chrome.browser.pdf.PdfUtils.PdfLoadResult;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.BeforeUnloadCallback;
import org.chromium.chrome.browser.ui.native_page.NativePageHost;
import org.chromium.chrome.modules.on_demand.OnDemandModule;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.embedder_support.util.UrlConstants;
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
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.ref.WeakReference;
import java.text.DateFormat;
import java.util.Date;
import java.util.Locale;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;
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
    private @TriState int mIsFitToPageActive;
    private float mLastFitZoom = -1f;

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

    private @Nullable AlertDialog mAlertDialog;

    /**
     * Whether the pdf has been loaded, despite of success or failure, for the current mUri. This is
     * used to ensure we load the pdf at most once. If mUri was updated, this is reset to false.
     */
    private boolean mIsPdfLoaded;

    private boolean mHasMadeAnyChanges;

    boolean mIsInitialZoomPass = true;
    private boolean mIsDefaultZoomPending;

    private int mFindInPageCount;

    private boolean mPageNavAndEditVisible = true;

    @VisibleForTesting public ChromePdfViewerFragment mChromePdfViewerFragment;
    private final Tab mTab;
    private @Nullable PropertyModel mModalDialogModel;
    private @Nullable Runnable mAlertDialogCancelRunnable;
    private boolean mDownloadAfterSave;
    private boolean mIsEditModeActive;
    private final ObserverList<Observer> mObservers = new ObserverList<>();
    private final BeforeUnloadCallback mBeforeUnloadCallback =
            new BeforeUnloadCallback() {
                @Override
                public boolean handleBeforeUnload(Runnable onProceed, Runnable onCancel) {
                    if (hasChanges()) {
                        showLeaveConfirmationDialog(onProceed, onCancel);
                        return true;
                    }
                    return false;
                }
            };

    /**
     * Creates a PdfCoordinator for the PdfPage.
     *
     * @param host A NativePageHost to load urls.
     * @param profile The current Profile.
     * @param activity The current Activity.
     * @param filepath The pdf filepath.
     * @param title The pdf title.
     * @param tab The tab.
     * @param url The url of the pdf.
     */
    public PdfCoordinator(
            NativePageHost host,
            Profile profile,
            Activity activity,
            @Nullable String filepath,
            String title,
            Tab tab,
            String url,
            PdfFragmentViewTracker pdfFragmentViewTracker) {
        mActivity = activity;
        mTab = tab;
        mTabId = String.valueOf(tab.getId());
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
            if (fragmentContainerView != null) {
                mFragmentContainerViewId = View.generateViewId();
                fragmentContainerView.setId(mFragmentContainerViewId);
            } else {
                mFragmentContainerViewId = R.id.pdf_fragment_container;
            }
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
        mTab.getUserDataHost().setUserData(BeforeUnloadCallback.class, mBeforeUnloadCallback);
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
        // TODO(crbug.com/536943332): Track and restore the calculated current page instead of
        // only saving the first visible page on exit edit mode or save instance state.
        private int mSavedPageIndex = -1;
        private float mSavedZoom = -1f;
        private boolean mRestorePositionPending;
        private @Nullable View mToolBoxView;
        private @Nullable ViewGroup mContainerView;
        private int mOriginalIndex;
        private boolean mShowToolBoxView = true;
        private boolean mTwoPagesPerRowEnabled;
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
                maybeRestorePosition();
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
                    (firstVisiblePage, visiblePagesCount, pageLocations, zoomLevel) -> {
                        maybeRestorePosition();
                        delegate.onViewportChanged(
                                calculateCurrentPage(capturedView, firstVisiblePage, pageLocations),
                                zoomLevel);
                    });
        }

        private void maybeRestorePosition() {
            if (mRestorePositionPending && mPdfView != null) {
                mRestorePositionPending = false;
                final float zoom = mSavedZoom;
                final int page = mSavedPageIndex;
                mPdfView.post(
                        () -> {
                            if (zoom > 0) {
                                zoomTo(zoom);
                            }
                            if (page >= 0) {
                                scrollToPage(page);
                            }
                        });
            }
        }

        @VisibleForTesting
        static int calculateCurrentPage(
                PdfView pdfView, int firstVisiblePage, @Nullable SparseArray<RectF> pageLocations) {
            int currentPage = firstVisiblePage;
            if (pageLocations != null && pdfView.getHeight() > 0) {
                float threshold = pdfView.getHeight() / 2.0f;
                RectF prevRect = null;
                for (int i = 0; i < pageLocations.size(); i++) {
                    int pageIndex = pageLocations.keyAt(i);
                    RectF rect = pageLocations.valueAt(i);
                    boolean isNewRow =
                            prevRect == null
                                    || rect.left <= prevRect.left
                                    || rect.top >= prevRect.bottom;
                    if (isNewRow) {
                        if (rect.top <= threshold) {
                            currentPage = pageIndex;
                        } else {
                            break;
                        }
                    }
                    prevRect = rect;
                }
            }
            return currentPage;
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
            if (getView() != null && mViewTag != null) {
                getView().setTag(mViewTag);
            }
            if (savedInstanceState != null) {
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
                try (Cursor cursor = contentResolver.query(uri, projection, null, null, null)) {
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
                        | SQLException e) {
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
            PdfUtils.recordEditFabAction();
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
            if (getContext() == null || mDelegate == null) {
                cleanupWriteResources(null, handle);
                setEditModeEnabled(false);
                return;
            }

            ParcelFileDescriptor pfd = null;
            boolean success = false;
            File tempFile = null;
            // TODO(crbug.com/554018452): Revisit how the fragment accesses host and PDF data.
            final boolean isIncognito = mDelegate.isIncognito();
            try {
                if (isIncognito) {
                    FileDescriptor fd = Os.memfd_create("annotated_pdf", 0);
                    pfd = ParcelFileDescriptor.dup(fd);
                    try {
                        Os.close(fd);
                    } catch (Exception ignored) {
                    }
                } else {
                    // Create a temporary file to write the annotated PDF to.
                    // This prevents truncating the original PDF file while the PDF library is
                    // reading
                    // from it.
                    File cacheDir = getContext().getCacheDir();
                    File pdfsDir = new File(cacheDir, "pdfs");
                    if (!pdfsDir.exists()) {
                        pdfsDir.mkdirs();
                    }
                    tempFile = File.createTempFile("annotated_", ".pdf", pdfsDir);
                    pfd =
                            ParcelFileDescriptor.open(
                                    tempFile,
                                    ParcelFileDescriptor.MODE_READ_WRITE
                                            | ParcelFileDescriptor.MODE_CREATE
                                            | ParcelFileDescriptor.MODE_TRUNCATE);
                }

                if (pfd != null) {
                    final ParcelFileDescriptor finalPfd = pfd;
                    final File finalTempFile = tempFile;
                    final AtomicBoolean handled = new AtomicBoolean(false);

                    Consumer<Boolean> onSaveFinished =
                            (isSuccess) -> {
                                if (!handled.compareAndSet(false, true)) return;
                                PostTask.postTask(
                                        TaskTraits.USER_BLOCKING_MAY_BLOCK,
                                        () -> {
                                            ParcelFileDescriptor savedPfd = null;
                                            if (isSuccess && isIncognito) {
                                                try {
                                                    savedPfd = finalPfd.dup();
                                                } catch (IOException e) {
                                                    Log.e(
                                                            TAG,
                                                            "Failed to dup ParcelFileDescriptor",
                                                            e);
                                                }
                                            }
                                            cleanupWriteResources(finalPfd, handle);
                                            if (isSuccess
                                                    && (isIncognito
                                                            ? (savedPfd != null)
                                                            : (finalTempFile != null
                                                                    && finalTempFile.length()
                                                                            > 0))) {
                                                final ParcelFileDescriptor finalSavedPfd = savedPfd;
                                                if (mDelegate != null) {
                                                    mDelegate.onPdfEditsSaved(
                                                            finalTempFile,
                                                            finalSavedPfd,
                                                            () -> {
                                                                finishExitingEditMode();
                                                            });
                                                } else {
                                                    if (finalSavedPfd != null) {
                                                        try {
                                                            finalSavedPfd.close();
                                                        } catch (IOException ignored) {
                                                        }
                                                    }
                                                    finishExitingEditMode();
                                                }
                                            } else {
                                                if (savedPfd != null) {
                                                    try {
                                                        savedPfd.close();
                                                    } catch (IOException ignored) {
                                                    }
                                                }
                                                if (finalTempFile != null) {
                                                    finalTempFile.delete();
                                                }
                                                if (mDelegate != null) {
                                                    mDelegate.onPdfEditsSaveFailed();
                                                }
                                                ThreadUtils.postOnUiThread(
                                                        () -> {
                                                            finishExitingEditMode();
                                                        });
                                            }
                                        });
                            };

                    Continuation<Unit> continuation =
                            new Continuation<Unit>() {
                                @Override
                                public CoroutineContext getContext() {
                                    return EmptyCoroutineContext.INSTANCE;
                                }

                                @Override
                                public void resumeWith(Object result) {
                                    boolean isSuccess = result == Unit.INSTANCE;
                                    if (!isSuccess) {
                                        Log.e(TAG, "Async PDF write failed: " + result);
                                    }
                                    onSaveFinished.accept(isSuccess);
                                }
                            };

                    if (mPdfView != null) {
                        mSavedPageIndex = mPdfView.getFirstVisiblePage();
                        mSavedZoom = mPdfView.getZoom();
                        mRestorePositionPending = true;
                    }

                    Object coroutineResult = handle.writeTo(pfd, continuation);

                    if (coroutineResult != IntrinsicsKt.getCOROUTINE_SUSPENDED()) {
                        // Completed synchronously.
                        boolean isSuccess = (coroutineResult == Unit.INSTANCE);
                        onSaveFinished.accept(isSuccess);
                    }
                    success = true;
                    return;
                } else {
                    Log.e(
                            TAG,
                            "Failed to open file descriptor for writing: "
                                    + (isIncognito ? "memfd" : tempFile));
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to write PDF edits", e);
            } finally {
                if (!success) {
                    cleanupWriteResources(pfd, handle);
                    if (tempFile != null) {
                        tempFile.delete();
                    }
                    if (mDelegate != null) {
                        mDelegate.onPdfEditsSaveFailed();
                    }
                    ThreadUtils.postOnUiThread(
                            () -> {
                                finishExitingEditMode();
                            });
                }
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
            if (mDelegate != null) {
                mDelegate.onPdfEditsSaveFailed();
            }
        }

        @Override
        public void onSaveInstanceState(Bundle outState) {
            super.onSaveInstanceState(outState);
            outState.putString(KEY_VIEW_TAG, mViewTag);
            if (mPdfView != null) {
                mSavedPageIndex = mPdfView.getFirstVisiblePage();
                mSavedZoom = mPdfView.getZoom();
                mRestorePositionPending = true;
            }
            outState.putInt(KEY_SAVED_PAGE_INDEX, mSavedPageIndex);
            outState.putFloat(KEY_SAVED_ZOOM, mSavedZoom);
            outState.putBoolean(KEY_RESTORE_POSITION_PENDING, mRestorePositionPending);
            outState.putString(KEY_FILE_PATH, mFilePath);
            outState.putString(KEY_FILE_NAME, mFileName);
        }

        @Override
        public boolean onLinkClicked(ExternalLink externalLink) {
            if (mDelegate != null) {
                mDelegate.onLinkClicked(externalLink.getUri());
            }
            // Always return true to consume the click event, preventing androidx.pdf from
            // falling back to its internal startActivity() call.
            return true;
        }

        @Override
        public void onLoadDocumentSuccess(PdfDocument pdfDocument) {
            super.onLoadDocumentSuccess(pdfDocument);
            if (!PdfUtils.isInlinePdfV2Enabled()) {
                maybeHideToolBoxForUnsupportedEdit();
            } else if (!PdfUtils.isInlinePdfV2EditEnabled()) {
                hideToolBox();
            }
            if (PdfUtils.isInlinePdfV2Enabled() && mPdfView != null) {
                mPdfView.setFormFillingEnabled(
                        PdfUtils.isInlinePdfV2FormFillingEnabled() && !isEditModeEnabled());
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

        void setDefaultZoom(int pageIndex, @Nullable Consumer<Float> onComplete) {
            PdfView pdfView = mPdfView;
            if (pdfView == null) {
                if (onComplete != null) {
                    PostTask.postTask(TaskTraits.UI_DEFAULT, () -> onComplete.accept(1.0f));
                }
                return;
            }

            // 1. Get the viewport width in actual screen pixels
            int viewportWidthPx =
                    pdfView.getWidth() - pdfView.getPaddingLeft() - pdfView.getPaddingRight();

            // 2. Convert screen pixels directly to DP using Android's density
            float density = pdfView.getContext().getResources().getDisplayMetrics().density;
            float viewportWidthDp = viewportWidthPx / density;

            runWithPageInfo(
                    pageIndex,
                    pageInfo -> {
                        if (pageInfo == null) {
                            if (onComplete != null) {
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT, () -> onComplete.accept(1.0f));
                            }
                            return;
                        }
                        float newZoom =
                                calculateFitToPageZoom(
                                        pageInfo,
                                        /* fitToPage= */ false,
                                        pdfView,
                                        /* zoomRatio= */ viewportWidthDp >= 600 ? 0.5f : 1.0f);
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    float zoomToReport = newZoom;
                                    if (!mRestorePositionPending) {
                                        pdfView.setZoom(newZoom);
                                    } else {
                                        zoomToReport = pdfView.getZoom();
                                    }
                                    pdfView.setHorizontalPageSpacing(2);
                                    pdfView.setVerticalPageSpacing(2);
                                    if (onComplete != null) {
                                        onComplete.accept(zoomToReport);
                                    }
                                });
                    });
        }


        void setPagesPerRow(boolean twoPagesPerRowEnabled) {
            mTwoPagesPerRowEnabled = twoPagesPerRowEnabled;
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
                PageInfo info, boolean fitToPage, PdfView pdfView, float zoomRatio) {
            int contentWidth = info.getWidth();
            int contentHeight = info.getHeight();
            if (contentWidth <= 0 || (fitToPage && contentHeight <= 0)) return 0f;

            int viewportWidth =
                    pdfView.getWidth() - pdfView.getPaddingLeft() - pdfView.getPaddingRight();
            if (viewportWidth <= 0) return 0f;

            int pagesPerRow = mTwoPagesPerRowEnabled ? 2 : 1;
            int totalContentWidth = contentWidth * pagesPerRow;
            float zoomWidth = ((float) viewportWidth * zoomRatio) / totalContentWidth;
            float newZoom = zoomWidth;

            if (fitToPage) {
                int viewportHeight =
                        pdfView.getHeight() - pdfView.getPaddingTop() - pdfView.getPaddingBottom();
                if (viewportHeight <= 0) return 0f;
                float zoomHeight = ((float) viewportHeight * zoomRatio) / contentHeight;
                newZoom = Math.min(zoomWidth, zoomHeight);
            }

            return Math.max(pdfView.getMinZoom(), Math.min(newZoom, pdfView.getMaxZoom()));
        }

        private void runWithPageInfo(int pageIndex, Consumer<@Nullable PageInfo> action) {
            PdfView pdfView = mPdfView;
            if (pdfView == null) {
                action.accept(null);
                return;
            }

            // pdfDocument can legitimately be null during tab teardown or concurrent switches.
            PdfDocument pdfDocument = pdfView.getPdfDocument();
            if (pdfDocument == null) {
                action.accept(null);
                return;
            }

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
                                    action.accept(null);
                                }
                            }
                        });
            } catch (PdfDocument.DocumentClosedException e) {
                Log.w(TAG, "Failed to get page info", e);
                action.accept(null);
            }
        }

        void fitToPage(boolean fitToPage, int pageIndex) {
            fitToPage(fitToPage, pageIndex, null);
        }

        void fitToPage(boolean fitToPage, int pageIndex, @Nullable Consumer<Float> onComplete) {
            PdfView pdfView = mPdfView;
            if (pdfView == null) {
                if (onComplete != null) {
                    PostTask.postTask(TaskTraits.UI_DEFAULT, () -> onComplete.accept(-1f));
                }
                return;
            }

            runWithPageInfo(
                    pageIndex,
                    pageInfo -> {
                        if (pageInfo == null) {
                            if (onComplete != null) {
                                PostTask.postTask(
                                        TaskTraits.UI_DEFAULT, () -> onComplete.accept(-1f));
                            }
                            return;
                        }
                        float newZoom =
                                calculateFitToPageZoom(
                                        pageInfo, fitToPage, pdfView, /* zoomRatio= */ 1.0f);
                        PostTask.postTask(
                                TaskTraits.UI_DEFAULT,
                                () -> {
                                    if (onComplete != null) {
                                        onComplete.accept(newZoom);
                                    }
                                    pdfView.setZoom(newZoom);
                                    // Scroll to the top of the page after zooming.
                                    scrollToPage(pageIndex);
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
            view.setOnClickListener(
                    v -> {
                        PdfUtils.recordEditFabAction();
                        openPdfInExternalEditor();
                    });
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
        if (mTab != null && !mTab.isDestroyed()) {
            try {
                if (mTab.getUserDataHost().getUserData(BeforeUnloadCallback.class)
                        == mBeforeUnloadCallback) {
                    mTab.getUserDataHost().removeUserData(BeforeUnloadCallback.class);
                }
            } catch (IllegalStateException ignored) {
                // UserDataHost was already destroyed or key was already removed.
            }
        }
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.destroy();
        }
        if (mModalDialogModel != null && mActivity instanceof ModalDialogManagerHolder) {
            ModalDialogManager modalDialogManager =
                    ((ModalDialogManagerHolder) mActivity).getModalDialogManager();
            if (modalDialogManager != null) {
                modalDialogManager.dismissDialog(
                        mModalDialogModel, DialogDismissalCause.ACTIVITY_DESTROYED);
            }
            mModalDialogModel = null;
        }
        if (mAlertDialog != null) {
            mAlertDialogCancelRunnable = null;
            mAlertDialog.dismiss();
            mAlertDialog = null;
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

    /**
     * Updates the PDF file path and URI after changes are saved.
     *
     * @param tempFile The temporary file containing the saved PDF content (non-Incognito).
     * @param pfd The ParcelFileDescriptor containing the saved PDF content in memory (Incognito).
     * @param onDone The callback to execute when the update has completed.
     */
    private void updatePdfAfterSave(
            @Nullable File tempFile, @Nullable ParcelFileDescriptor pfd, Runnable onDone) {
        if (mIsIncognito) {
            int fd = -1;
            if (pfd != null) {
                fd = pfd.detachFd();
            } else if (tempFile != null) {
                try {
                    // In incognito, adopt the new temporary file descriptor into the
                    // PdfContentProvider so that it matches the secure sharing design.
                    ParcelFileDescriptor openedPfd =
                            ParcelFileDescriptor.open(
                                    tempFile, ParcelFileDescriptor.MODE_READ_ONLY);
                    fd = openedPfd.detachFd();
                } catch (IOException e) {
                    Log.e(TAG, "Failed to open temporary PDF file descriptor", e);
                } finally {
                    // Delete temp file from disk immediately to ensure incognito privacy and avoid
                    // disk leaks.
                    if (tempFile.exists()) {
                        tempFile.delete();
                    }
                }
            }

            final int finalFd = fd;
            ThreadUtils.postOnUiThread(
                    () -> {
                        if (finalFd >= 0) {
                            String securePath = "/proc/self/fd/" + finalFd;
                            Uri newUri =
                                    PdfContentProvider.registerStream(mTabId, securePath, mTitle);
                            try {
                                ParcelFileDescriptor.adoptFd(finalFd).close();
                            } catch (IOException e) {
                                /* ignore */
                            }
                            if (newUri != null) {
                                PdfContentProvider.removeContentUri(mPdfFilePath);
                                mPdfFilePath = newUri.toString();
                                mUri = newUri;
                                if (mChromePdfViewerFragment != null) {
                                    mChromePdfViewerFragment.setDocumentUri(mUri);
                                    mChromePdfViewerFragment.setFilePath(mPdfFilePath);
                                    mChromePdfViewerFragment.setFileName(mTitle);
                                }
                            }
                        }
                        onDone.run();
                    });
        } else {
            if (tempFile == null) {
                if (pfd != null) {
                    try {
                        pfd.close();
                    } catch (IOException ignored) {
                    }
                }
                onDone.run();
                return;
            }
            boolean overwritten = false;
            String updatedFilePath = null;
            if (mPdfFilePath != null) {
                Uri uri = Uri.parse(mPdfFilePath);
                String scheme = uri.getScheme();
                if (UrlConstants.FILE_SCHEME.equals(scheme)) {
                    String path = uri.getPath();
                    if (path != null) {
                        File originalFile = new File(path);
                        overwritten = overwriteOriginalFile(tempFile, originalFile);
                        if (overwritten) {
                            updatedFilePath = originalFile.getAbsolutePath();
                        }
                    }
                } else if (UrlConstants.CONTENT_SCHEME.equals(scheme)) {
                    overwritten = overwriteOriginalContentUri(tempFile, uri);
                } else {
                    File originalFile = new File(mPdfFilePath);
                    overwritten = overwriteOriginalFile(tempFile, originalFile);
                    if (overwritten) {
                        updatedFilePath = originalFile.getAbsolutePath();
                    }
                }
            }

            if (overwritten) {
                if (tempFile.exists()) {
                    tempFile.delete();
                }
            } else {
                updatedFilePath = tempFile.getAbsolutePath();
            }

            final String finalUpdatedFilePath = updatedFilePath;
            ThreadUtils.postOnUiThread(
                    () -> {
                        if (finalUpdatedFilePath != null) {
                            mPdfFilePath = finalUpdatedFilePath;
                            mUri = PdfUtils.getUriFromFilePath(mPdfFilePath);
                        }
                        onDone.run();
                    });
        }
    }

    private boolean overwriteOriginalFile(File tempFile, File originalFile) {
        try {
            if (tempFile.renameTo(originalFile)) {
                return true;
            }
            try (FileInputStream is = new FileInputStream(tempFile);
                    FileOutputStream os = new FileOutputStream(originalFile)) {
                FileUtils.copyStream(is, os);
                return true;
            }
        } catch (IOException | SecurityException e) {
            Log.e(TAG, "No write permission for original file path: " + originalFile.getPath(), e);
            return false;
        }
    }

    private boolean overwriteOriginalContentUri(File tempFile, Uri uri) {
        if (mActivity == null) return false;
        try (FileInputStream is = new FileInputStream(tempFile);
                OutputStream os = mActivity.getContentResolver().openOutputStream(uri, "w")) {
            if (os != null) {
                FileUtils.copyStream(is, os);
                return true;
            } else {
                return false;
            }
        } catch (SecurityException | IllegalArgumentException e) {
            Log.e(TAG, "No write permission for original URI", e);
            return false;
        } catch (IOException e) {
            Log.e(TAG, "IO exception when writing to original URI", e);
            return false;
        }
    }

    private void loadPdfFile(@Nullable String pdfFilePath) {
        mPdfFilePath = pdfFilePath;
        loadPdfFile();
    }

    @Override
    public void resetLoadState() {
        mIsPdfLoaded = false;
        mIsFitToPageActive = TriState.NOT_SET;
        mLastFitZoom = -1f;
        mHasMadeAnyChanges = false;
        if (mChromePdfViewerFragment != null) {
            if (mChromePdfViewerFragment.isAdded()) {
                mChromePdfViewerFragment.setDocumentUri(null);
                mChromePdfViewerFragment.setEditModeEnabled(false);
            }
            mChromePdfViewerFragment.setPagesPerRow(false);
        }
        // Reset two-pages-per-row state early so the overflow menu doesn't show a stale label while
        // loading, and to prevent permanent out-of-sync state if loading fails or is aborted before
        // onDocumentLoaded() is invoked.
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.resetTwoPagesPerRow();
            mToolbarCoordinator.setEditModeActive(false);
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

        if (hasChanges()) {
            showReloadConfirmationDialog(this::performReload);
        } else {
            performReload();
        }
    }

    @Override
    public boolean hasChanges() {
        if (mHasMadeAnyChanges) {
            return true;
        }
        if (mChromePdfViewerFragment != null && mChromePdfViewerFragment.isAdded()) {
            try {
                return mChromePdfViewerFragment.hasUnsavedChanges() || isEditModeActive();
            } catch (IllegalStateException e) {
                return false;
            }
        }
        return false;
    }

    @Override
    public void showReloadConfirmationDialog(Runnable onConfirm) {
        if (mActivity == null || mActivity.isFinishing() || mActivity.isDestroyed()) {
            return;
        }
        ModalDialogManager modalDialogManager = null;
        Runnable onConfirmWithMetric =
                () -> {
                    PdfUtils.recordDiscardAnnotations();
                    if (onConfirm != null) {
                        onConfirm.run();
                    }
                };
        if (mActivity instanceof ModalDialogManagerHolder) {
            modalDialogManager = ((ModalDialogManagerHolder) mActivity).getModalDialogManager();
        }
        if (modalDialogManager != null) {
            showUnsavedChangesModalDialog(
                    modalDialogManager,
                    R.string.pdf_unsaved_changes_dialog_reload_title,
                    R.string.pdf_unsaved_changes_dialog_reload_button,
                    onConfirmWithMetric,
                    /* onCancel= */ null);
        } else {
            showUnsavedChangesAlertDialog(
                    R.string.pdf_unsaved_changes_dialog_reload_title,
                    R.string.pdf_unsaved_changes_dialog_reload_button,
                    onConfirmWithMetric,
                    /* onCancel= */ null);
        }
    }

    private void performReload() {
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
        mHasMadeAnyChanges = false;
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
                    mIsDefaultZoomPending = false;
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
     * Changes the zoom level of the PDF page.
     *
     * @param decrease Whether to decrease the zoom level.
     * @return True if the PDF page can be zoomed out, false otherwise.
     */
    @Override
    public boolean changeZoomLevel(boolean decrease) {
        if (mToolbarCoordinator == null) return false;
        Float nextZoomLevel = mToolbarCoordinator.getNextEngineZoomLevel(/* increase= */ !decrease);
        if (nextZoomLevel == null) return false;
        changeZoomLevel(nextZoomLevel);
        return true;
    }

    /**
     * Resets the zoom level of the PDF page to the default zoom level.
     *
     * @return True if the PDF page was zoomed to the default zoom level, false otherwise.
     */
    @Override
    public boolean resetZoomLevel() {
        if (mToolbarCoordinator == null) return false;
        float defaultZoomLevel = mToolbarCoordinator.getDefaultZoomLevel();
        if (defaultZoomLevel <= 0f) return false;
        changeZoomLevel(defaultZoomLevel);
        return true;
    }

    /**
     * Sets the zoom level to a specified amount.
     *
     * @param zoomLevel The new value of the zoom.
     */
    @Override
    public void changeZoomLevel(float zoomLevel) {
        mIsFitToPageActive = TriState.NOT_SET;
        mLastFitZoom = -1f;
        mChromePdfViewerFragment.zoomTo(zoomLevel);
    }

    /** Enters edit mode if Inline PDF V2 is enabled. */
    @Override
    public void enterEditMode() {
        if (!PdfUtils.isInlinePdfV2EditEnabled()) {
            return;
        }
        mChromePdfViewerFragment.setEditModeEnabled(true);
    }

    /** Exits edit mode and applies any draft edits. */
    @Override
    public void exitEditMode() {
        if (!PdfUtils.isInlinePdfV2EditEnabled() || mChromePdfViewerFragment == null) {
            return;
        }
        if (hasUnsavedChanges()) {
            mChromePdfViewerFragment.applyDraftEdits();
        } else {
            mChromePdfViewerFragment.setEditModeEnabled(false);
        }
    }

    /**
     * Toggles between "fit to page" and "fit to page width" modes.
     *
     * @param fitToPage Whether to fit to page or fit to page width.
     * @param pageIndex The 0-based index of page to update scaling.
     */
    @Override
    public void toggleFitToPage(boolean fitToPage, int pageIndex) {
        mIsFitToPageActive = fitToPage ? TriState.TRUE : TriState.FALSE;
        mLastFitZoom = -1f;
        mChromePdfViewerFragment.fitToPage(fitToPage, pageIndex, zoom -> mLastFitZoom = zoom);
    }

    @Override
    public void toggleTwoPagesPerRow(
            boolean twoPagesPerRowEnabled, float zoomLevel, int currentPageIndex) {
        assert mToolbarCoordinator != null;
        @TriState int previousFitState = mIsFitToPageActive;
        mLastFitZoom = -1f;
        mChromePdfViewerFragment.setPagesPerRow(twoPagesPerRowEnabled);
        if (previousFitState == TriState.NOT_SET) {
            mIsFitToPageActive = TriState.NOT_SET;
            mChromePdfViewerFragment.zoomTo(zoomLevel);
        } else {
            mIsFitToPageActive = previousFitState;
            mChromePdfViewerFragment.fitToPage(
                    previousFitState == TriState.TRUE,
                    currentPageIndex,
                    zoom -> mLastFitZoom = zoom);
        }
        mChromePdfViewerFragment.scrollToPage(currentPageIndex);
    }

    @VisibleForTesting
    boolean hasUnsavedChanges() {
        if (mChromePdfViewerFragment != null && mChromePdfViewerFragment.isAdded()) {
            try {
                return mChromePdfViewerFragment.hasUnsavedChanges();
            } catch (IllegalStateException e) {
                return false;
            }
        }
        return false;
    }

    private boolean isEditModeActive() {
        return mIsEditModeActive;
    }

    @Override
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
    }

    @Override
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public void download() {
        // Extract the download URL; null for local PDFs (e.g. file:// or content://).
        String downloadUrl = getDownloadUrl();

        // Re-download unmodified web/blob/data PDFs or fallback when V2 is disabled.
        // For unmodified local PDFs, downloadUrl is null and this is a no-op since the file is
        // already on the device.
        if (!PdfUtils.isInlinePdfV2Enabled() || !hasChanges()) {
            if (downloadUrl != null) {
                mNativePageHost.downloadUrl(downloadUrl);
            }
            return;
        }

        // If there are unsaved edits, flush draft edits first before triggering download.
        if (mChromePdfViewerFragment != null && (hasUnsavedChanges() || isEditModeActive())) {
            mDownloadAfterSave = true;
            mChromePdfViewerFragment.applyDraftEdits();
        } else {
            // Otherwise, export the annotated PDF file directly to the Downloads directory.
            downloadAnnotatedPdf();
        }
    }

    private @Nullable String getDownloadUrl() {
        String downloadUrl = PdfUtils.getPdfReDownloadUrl(mUrl);
        if (downloadUrl != null) {
            return downloadUrl;
        }
        String decodedUrl = PdfUtils.decodePdfPageUrl(mUrl);
        String candidateUrl = decodedUrl != null ? decodedUrl : mUrl;
        if (candidateUrl != null) {
            GURL gurl = new GURL(candidateUrl);
            if (gurl.isValid()) {
                String scheme = gurl.getScheme();
                if (UrlConstants.BLOB_SCHEME.equalsIgnoreCase(scheme)
                        || UrlConstants.DATA_SCHEME.equalsIgnoreCase(scheme)) {
                    return candidateUrl;
                }
            }
        }
        return null;
    }

    @Override
    public void print() {
        mNativePageHost.print();
    }

    private void downloadAnnotatedPdf() {
        String sourcePath = mUri != null ? mUri.toString() : mPdfFilePath;
        if (sourcePath == null || mActivity == null) return;
        Context context = mActivity.getApplicationContext();
        String filename = sanitizePdfFileName(mTitle);
        final String finalFilename = filename;
        final String finalSourcePath = sourcePath;

        Toast.makeText(context, R.string.pdf_downloading, Toast.LENGTH_SHORT).show();

        PostTask.postTask(
                TaskTraits.USER_BLOCKING_MAY_BLOCK,
                () -> {
                    try {
                        ContentValues values = new ContentValues();
                        values.put(MediaColumns.DISPLAY_NAME, finalFilename);
                        values.put(MediaColumns.MIME_TYPE, MimeTypeUtils.PDF_MIME_TYPE);
                        values.put(MediaColumns.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS);
                        values.put(MediaColumns.IS_PENDING, 1);

                        ContentResolver resolver = context.getContentResolver();
                        Uri uri = resolver.insert(Downloads.EXTERNAL_CONTENT_URI, values);
                        if (uri != null) {
                            boolean copied = false;
                            try {
                                try (InputStream is =
                                                openInputStreamForPath(context, finalSourcePath);
                                        OutputStream os = resolver.openOutputStream(uri)) {
                                    if (is != null && os != null) {
                                        FileUtils.copyStream(is, os);
                                        copied = true;
                                    }
                                }
                                if (copied) {
                                    values.clear();
                                    values.put(MediaColumns.IS_PENDING, 0);
                                    resolver.update(uri, values, null, null);
                                } else {
                                    resolver.delete(uri, null, null);
                                }
                            } catch (Exception e) {
                                resolver.delete(uri, null, null);
                                throw e;
                            }
                        }
                    } catch (Exception e) {
                        Log.e(TAG, "Failed to download PDF", e);
                    }
                });
    }

    private static String sanitizePdfFileName(@Nullable String title) {
        String filename = (title != null && !title.trim().isEmpty()) ? title.trim() : "document";
        filename = new File(filename).getName().replaceAll("[\\\\/:*?\"<>|]", "_");
        if (filename.isEmpty() || filename.equals(".") || filename.equals("..")) {
            filename = "document";
        }
        if (!filename.toLowerCase(Locale.ROOT).endsWith(".pdf")) {
            filename += ".pdf";
        }
        return filename;
    }

    private static @Nullable InputStream openInputStreamForPath(Context context, String path) {
        try {
            Uri uri = Uri.parse(path);
            if (UrlConstants.CONTENT_SCHEME.equals(uri.getScheme())) {
                return context.getContentResolver().openInputStream(uri);
            }
            String filePath =
                    UrlConstants.FILE_SCHEME.equals(uri.getScheme()) ? uri.getPath() : path;
            if (filePath != null) {
                File file = new File(filePath);
                if (file.exists()) {
                    return new FileInputStream(file);
                }
            }
            return context.getContentResolver().openInputStream(uri);
        } catch (Exception e) {
            Log.e(TAG, "Failed to open input stream for path: " + path, e);
        }
        return null;
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
        }
    }

    @Override
    public boolean onLinkClicked(Uri uri) {
        String scheme = uri.getScheme();
        if (scheme == null || !ALLOWED_LINK_SCHEMES.contains(scheme.toLowerCase(Locale.ROOT))) {
            PdfUtils.recordHyperlinkClickResult(PdfHyperlinkClickResult.BLOCKED_INVALID_SCHEME);
            return false;
        }
        LoadUrlParams params = new LoadUrlParams(uri.toString(), PAGE_TRANSITION_TYPE);
        params.setIsRendererInitiated(true);
        // TODO(crbug.com/484103003): Reconsider initiator origin if renderer initiated is true.
        params.setInitiatorOrigin(Origin.create(new GURL(mUrl)));
        // TODO(crbug.com/548013417): Reuse existing tab for link clicks.
        mNativePageHost.openNewTab(params);
        PdfUtils.recordHyperlinkClickResult(PdfHyperlinkClickResult.SUCCESS_LOAD_INITIATED);
        return true;
    }

    @Override
    public void onDocumentLoaded(int pageCount) {
        if (mToolbarCoordinator != null && mTitle != null) {
            mToolbarCoordinator.onDocumentLoaded(pageCount, mTitle);
        }
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
        mIsEditModeActive = editMode;
        if (mToolbarCoordinator != null) {
            mToolbarCoordinator.setEditModeActive(editMode);
        }
    }

    @Override
    public boolean isPageNavAndEditVisible() {
        return mPageNavAndEditVisible;
    }

    @Override
    public boolean isIncognito() {
        return mIsIncognito;
    }

    @Override
    public void onEditsApplied() {
        mHasMadeAnyChanges = true;
        for (Observer observer : mObservers) {
            observer.onHasChangesChanged();
        }
    }

    @Override
    public void onPdfEditsSaved(
            @Nullable File tempFile, @Nullable ParcelFileDescriptor pfd, Runnable onDone) {
        updatePdfAfterSave(
                tempFile,
                pfd,
                () -> {
                    onEditsApplied();
                    if (onDone != null) {
                        onDone.run();
                    }
                    if (mDownloadAfterSave) {
                        mDownloadAfterSave = false;
                        download();
                    }
                });
    }

    @Override
    public void onPdfEditsSaveFailed() {
        mDownloadAfterSave = false;
    }

    @Override
    public void onViewportChanged(int pageIndex, float zoomLevel) {
        assert mToolbarCoordinator != null;
        // AndroidX PDF Viewport is not initialized to 100% zoom on the initial pass. For PDF V2, we
        // hide the view until the first viewport change is detected and set to 100% zoom.
        if (PdfUtils.isInlinePdfV2Enabled() && mIsInitialZoomPass) {
            if (mIsDefaultZoomPending) {
                return;
            }
            ChromePdfViewerFragment fragment = mChromePdfViewerFragment;
            if (fragment != null) {
                PdfView pdfView = fragment.mPdfView;
                if (pdfView != null && pdfView.getPdfDocument() != null && pdfView.getWidth() > 0) {
                    mIsDefaultZoomPending = true;
                    fragment.setDefaultZoom(
                            pageIndex,
                            (defaultZoom) -> {
                                mIsInitialZoomPass = false;
                                mIsDefaultZoomPending = false;
                                if (mToolbarCoordinator != null) {
                                    mToolbarCoordinator.setDefaultZoomLevel(defaultZoom);
                                    mToolbarCoordinator.onViewportChanged(pageIndex, defaultZoom);
                                }
                                View fragmentContainerView =
                                        mView.findViewById(mFragmentContainerViewId);
                                if (fragmentContainerView != null
                                        && fragmentContainerView.getVisibility() != View.VISIBLE) {
                                    fragmentContainerView.setVisibility(View.VISIBLE);
                                    pdfView.requestFocus();
                                }
                            });
                    return;
                }
            }
        }
        if (mIsFitToPageActive != TriState.NOT_SET
                && mLastFitZoom >= 0f
                && !MathUtils.areFloatsEqual(zoomLevel, mLastFitZoom)) {
            mIsFitToPageActive = TriState.NOT_SET;
            mLastFitZoom = -1f;
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
        String fallbackFileName = mTitle;
        String pdfFilePath = mPdfFilePath;
        WeakReference<PdfCoordinator> weakSelf = new WeakReference<>(this);

        mChromePdfViewerFragment.runWithPageInfo(
                0,
                pageInfo -> {
                    if (pageInfo == null) return;
                    // Fetch properties on a background thread to avoid UI thread block
                    PostTask.postTask(
                            TaskTraits.USER_VISIBLE,
                            () -> {
                                PdfDocumentPropertiesFetcher.DocProperties fileProps =
                                        PdfDocumentPropertiesFetcher.getDocProperties(
                                                appContext,
                                                uri,
                                                fallbackFileName,
                                                pdfFilePath,
                                                mIsIncognito);
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
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        if (mModalDialogModel == model) {
                            mModalDialogModel = null;
                        }
                    }

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

        mModalDialogModel = model;
        manager.showDialog(model, ModalDialogType.TAB);
    }

    private void showAlertDialog(View dialogView) {
        if (mAlertDialog != null) {
            if (mAlertDialogCancelRunnable != null) {
                Runnable cancelRunnable = mAlertDialogCancelRunnable;
                mAlertDialogCancelRunnable = null;
                cancelRunnable.run();
            }
            mAlertDialog.dismiss();
        }
        mAlertDialog =
                new AlertDialog.Builder(mActivity)
                        .setTitle(R.string.pdf_document_properties)
                        .setView(dialogView)
                        .setPositiveButton(
                                R.string.pdf_properties_close, (dialog, which) -> dialog.dismiss())
                        .setOnDismissListener(
                                dialog -> {
                                    if (mAlertDialog == dialog) {
                                        mAlertDialog = null;
                                    }
                                })
                        .show();
    }

    @Nullable AlertDialog getAlertDialogForTesting() {
        return mAlertDialog;
    }

    @Nullable PdfToolbarCoordinator getToolbarCoordinatorForTesting() {
        return mToolbarCoordinator;
    }

    @TriState
    int getIsFitToPageActiveForTesting() {
        return mIsFitToPageActive;
    }

    void setIsFitToPageActiveForTesting(@TriState int isFitToPageActive) {
        mIsFitToPageActive = isFitToPageActive;
    }

    float getLastFitZoomForTesting() {
        return mLastFitZoom;
    }

    void setLastFitZoomForTesting(float lastFitZoom) {
        mLastFitZoom = lastFitZoom;
    }

    private void showLeaveConfirmationDialog(Runnable onProceed, Runnable onCancel) {
        if (mActivity == null || mActivity.isFinishing() || mActivity.isDestroyed()) {
            onProceed.run();
            return;
        }
        ModalDialogManager modalDialogManager = null;
        if (mActivity instanceof ModalDialogManagerHolder) {
            modalDialogManager = ((ModalDialogManagerHolder) mActivity).getModalDialogManager();
        }
        if (mModalDialogModel != null && modalDialogManager != null) {
            modalDialogManager.dismissDialog(
                    mModalDialogModel, DialogDismissalCause.ACTION_ON_CONTENT);
            mModalDialogModel = null;
        }
        if (mAlertDialog != null) {
            if (mAlertDialogCancelRunnable != null) {
                Runnable cancelRunnable = mAlertDialogCancelRunnable;
                mAlertDialogCancelRunnable = null;
                cancelRunnable.run();
            }
            mAlertDialog.dismiss();
            mAlertDialog = null;
        }
        Runnable onProceedWithMetric =
                () -> {
                    PdfUtils.recordDiscardAnnotations();
                    if (onProceed != null) {
                        onProceed.run();
                    }
                };
        if (modalDialogManager != null) {
            showUnsavedChangesModalDialog(
                    modalDialogManager,
                    R.string.pdf_unsaved_changes_dialog_leave_title,
                    R.string.pdf_unsaved_changes_dialog_leave_button,
                    onProceedWithMetric,
                    onCancel);
        } else {
            showUnsavedChangesAlertDialog(
                    R.string.pdf_unsaved_changes_dialog_leave_title,
                    R.string.pdf_unsaved_changes_dialog_leave_button,
                    onProceedWithMetric,
                    onCancel);
        }
    }

    private void showUnsavedChangesModalDialog(
            ModalDialogManager manager,
            @StringRes int titleResId,
            @StringRes int positiveButtonResId,
            Runnable onProceed,
            @Nullable Runnable onCancel) {
        ModalDialogProperties.Controller controller =
                new ModalDialogProperties.Controller() {
                    @Override
                    public void onDismiss(
                            PropertyModel model, @DialogDismissalCause int dismissalCause) {
                        if (mModalDialogModel == model) {
                            mModalDialogModel = null;
                        }
                        if (onCancel != null
                                && dismissalCause != DialogDismissalCause.POSITIVE_BUTTON_CLICKED
                                && dismissalCause != DialogDismissalCause.ACTIVITY_DESTROYED
                                && dismissalCause != DialogDismissalCause.TAB_DESTROYED
                                && dismissalCause != DialogDismissalCause.WEB_CONTENTS_DESTROYED) {
                            onCancel.run();
                        }
                    }

                    @Override
                    public void onClick(PropertyModel model, int buttonType) {
                        if (buttonType == ModalDialogProperties.ButtonType.POSITIVE) {
                            manager.dismissDialog(
                                    model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                            onProceed.run();
                        } else if (buttonType == ModalDialogProperties.ButtonType.NEGATIVE) {
                            manager.dismissDialog(
                                    model, DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                        }
                    }
                };

        PropertyModel model =
                new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                        .with(ModalDialogProperties.CONTROLLER, controller)
                        .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                        .with(ModalDialogProperties.TITLE, mActivity.getString(titleResId))
                        .with(
                                ModalDialogProperties.MESSAGE_PARAGRAPH_1,
                                mActivity.getString(R.string.pdf_unsaved_changes_dialog_message))
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                mActivity.getResources(),
                                positiveButtonResId)
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                mActivity.getResources(),
                                R.string.pdf_unsaved_changes_dialog_cancel_button)
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                ModalDialogProperties.ButtonStyles.PRIMARY_FILLED_NEGATIVE_OUTLINE)
                        .build();

        mModalDialogModel = model;
        manager.showDialog(model, ModalDialogType.TAB);
    }

    private void showUnsavedChangesAlertDialog(
            @StringRes int titleResId,
            @StringRes int positiveButtonResId,
            Runnable onProceed,
            @Nullable Runnable onCancel) {
        if (mAlertDialog != null) {
            if (mAlertDialogCancelRunnable != null) {
                Runnable cancelRunnable = mAlertDialogCancelRunnable;
                mAlertDialogCancelRunnable = null;
                cancelRunnable.run();
            }
            mAlertDialog.dismiss();
            mAlertDialog = null;
        }
        mAlertDialogCancelRunnable = onCancel;
        mAlertDialog =
                new AlertDialog.Builder(mActivity)
                        .setTitle(titleResId)
                        .setMessage(R.string.pdf_unsaved_changes_dialog_message)
                        .setPositiveButton(
                                positiveButtonResId,
                                (dialog, which) -> {
                                    mAlertDialogCancelRunnable = null;
                                    dialog.dismiss();
                                    onProceed.run();
                                })
                        .setNegativeButton(
                                R.string.pdf_unsaved_changes_dialog_cancel_button,
                                (dialog, which) -> {
                                    mAlertDialogCancelRunnable = null;
                                    dialog.dismiss();
                                    if (onCancel != null) {
                                        onCancel.run();
                                    }
                                })
                        .setOnCancelListener(
                                dialog -> {
                                    if (mAlertDialogCancelRunnable != null) {
                                        Runnable cancelRunnable = mAlertDialogCancelRunnable;
                                        mAlertDialogCancelRunnable = null;
                                        cancelRunnable.run();
                                    }
                                })
                        .setOnDismissListener(
                                dialog -> {
                                    if (mAlertDialog == dialog) {
                                        mAlertDialog = null;
                                        mAlertDialogCancelRunnable = null;
                                    }
                                })
                        .show();
    }
}
