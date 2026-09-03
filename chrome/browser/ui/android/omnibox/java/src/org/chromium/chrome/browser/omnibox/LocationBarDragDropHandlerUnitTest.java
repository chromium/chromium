// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ClipData;
import android.content.ClipDescription;
import android.content.ContentProvider;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.view.DragAndDropPermissions;
import android.view.DragEvent;
import android.view.View;
import android.webkit.MimeTypeMap;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowContentResolver;
import org.robolectric.shadows.ShadowMimeTypeMap;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxLoadUrlParams;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.lang.ref.WeakReference;

/** Unit tests for {@link LocationBarDragDropHandler}. */
@RunWith(BaseRobolectricTestRunner.class)
public class LocationBarDragDropHandlerUnitTest {
    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private OmniboxStub mOmniboxStub;
    @Mock private LocationBarDataProvider mDataProvider;
    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroid;
    @Mock private DragEvent mDragEvent;
    @Mock private ClipDescription mClipDescription;
    @Mock private ClipData mClipData;
    @Mock private ClipData.Item mClipDataItem;
    @Mock private Activity mActivity;
    @Mock private DragAndDropPermissions mDragAndDropPermissions;
    @Mock private ContentProvider mContentProvider;
    @Mock private Intent mIntent;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<OmniboxLoadUrlParams> mLoadUrlParamsCaptor;

    private LocationBarDragDropHandler mHandler;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        lenient().when(mDataProvider.getTab()).thenReturn(mTab);
        lenient().when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        mHandler = new LocationBarDragDropHandler(mOmniboxStub, mDataProvider);

        ShadowMimeTypeMap shadowMimeTypeMap = Shadows.shadowOf(MimeTypeMap.getSingleton());
        shadowMimeTypeMap.addExtensionMimeTypeMapping("html", "text/html");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("png", "image/png");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("txt", "text/plain");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("pdf", "application/pdf");
    }

    @Test
    public void testOnDrag_DragStarted_ValidMimeType() {
        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(mDragEvent.getClipDescription()).thenReturn(mClipDescription);
        when(mClipDescription.getMimeTypeCount()).thenReturn(1);
        when(mClipDescription.getMimeType(0)).thenReturn("text/plain");

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, mDragEvent));
    }

    @Test
    public void testOnDrag_DragStarted_ChromeInternalMimeType() {
        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(mDragEvent.getClipDescription()).thenReturn(mClipDescription);
        when(mClipDescription.getMimeTypeCount()).thenReturn(1);
        when(mClipDescription.getMimeType(0)).thenReturn("chrome/tab");

        View view = new View(mContext);
        assertFalse(mHandler.onDrag(view, mDragEvent));
    }

    @Test
    public void testOnDrag_DragStarted_InvalidMimeType() {
        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(mDragEvent.getClipDescription()).thenReturn(mClipDescription);
        when(mClipDescription.getMimeTypeCount()).thenReturn(1);
        when(mClipDescription.getMimeType(0)).thenReturn("video/mp4");

        View view = new View(mContext);
        assertFalse(mHandler.onDrag(view, mDragEvent));
    }

    @Test
    public void testOnDrag_DragStarted_TabDragWithUriList_Accepted() {
        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(mDragEvent.getClipDescription()).thenReturn(mClipDescription);
        when(mClipDescription.hasMimeType(ClipDescription.MIMETYPE_TEXT_URILIST)).thenReturn(true);

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, mDragEvent));
    }

    @Test
    public void testOnDrag_Drop_FileUri() {

        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(mDragEvent.getClipData()).thenReturn(mClipData);
        when(mClipData.getItemCount()).thenReturn(1);
        when(mClipData.getItemAt(0)).thenReturn(mClipDataItem);

        Uri fileUri = Uri.parse("file:///path/to/file.txt");
        when(mClipDataItem.getUri()).thenReturn(fileUri);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, mDragEvent));

        verify(mTab, never()).addObserver(any());
        verify(mOmniboxStub).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals("file:///path/to/file.txt", mLoadUrlParamsCaptor.getValue().url);
    }

    @Test
    public void testOnDrag_Drop_ContentUri() {

        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(mDragEvent.getClipData()).thenReturn(mClipData);
        when(mClipData.getItemCount()).thenReturn(1);
        when(mClipData.getItemAt(0)).thenReturn(mClipDataItem);

        Uri contentUri = Uri.parse("content://com.example.provider/document.pdf");
        when(mClipDataItem.getUri()).thenReturn(contentUri);

        when(mContentProvider.getType(contentUri)).thenReturn("application/pdf");
        ShadowContentResolver.registerProviderInternal("com.example.provider", mContentProvider);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mActivity.requestDragAndDropPermissions(mDragEvent))
                .thenReturn(mDragAndDropPermissions);

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, mDragEvent));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mOmniboxStub).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(
                "content://com.example.provider/document.pdf", mLoadUrlParamsCaptor.getValue().url);

        verify(mDragAndDropPermissions, never()).release();

        mTabObserverCaptor.getValue().onPageLoadFinished(mTab, new GURL(contentUri.toString()));

        verify(mDragAndDropPermissions).release();
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testOnDrag_Drop_ContentUri_LoadFailed() {

        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(mDragEvent.getClipData()).thenReturn(mClipData);
        when(mClipData.getItemCount()).thenReturn(1);
        when(mClipData.getItemAt(0)).thenReturn(mClipDataItem);

        Uri contentUri = Uri.parse("content://com.example.provider/document.pdf");
        when(mClipDataItem.getUri()).thenReturn(contentUri);

        when(mContentProvider.getType(contentUri)).thenReturn("application/pdf");
        ShadowContentResolver.registerProviderInternal("com.example.provider", mContentProvider);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mActivity.requestDragAndDropPermissions(mDragEvent))
                .thenReturn(mDragAndDropPermissions);

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, mDragEvent));

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        mTabObserverCaptor.getValue().onPageLoadFailed(mTab, 0);

        verify(mDragAndDropPermissions).release();
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testOnDrag_Drop_ContentUri_TabDestroyed() {

        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(mDragEvent.getClipData()).thenReturn(mClipData);
        when(mClipData.getItemCount()).thenReturn(1);
        when(mClipData.getItemAt(0)).thenReturn(mClipDataItem);

        Uri contentUri = Uri.parse("content://com.example.provider/document.pdf");
        when(mClipDataItem.getUri()).thenReturn(contentUri);

        when(mContentProvider.getType(contentUri)).thenReturn("application/pdf");
        ShadowContentResolver.registerProviderInternal("com.example.provider", mContentProvider);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));
        when(mActivity.requestDragAndDropPermissions(mDragEvent))
                .thenReturn(mDragAndDropPermissions);

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, mDragEvent));

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        mTabObserverCaptor.getValue().onDestroyed(mTab);

        verify(mDragAndDropPermissions).release();
    }

    @Test
    public void testIsAcceptableMimeType() {
        assertTrue(mHandler.isAcceptableMimeType("text/plain"));
        assertTrue(mHandler.isAcceptableMimeType("image/png"));
        assertTrue(mHandler.isAcceptableMimeType("application/pdf"));
        assertFalse(mHandler.isAcceptableMimeType("video/mp4"));
        assertFalse(mHandler.isAcceptableMimeType("text/uri-list"));
        assertFalse(mHandler.isAcceptableMimeType(null));
    }

    @Test
    public void testGetMimeType() {
        // Content URI
        Uri contentUri = Uri.parse("content://com.example.provider/doc");
        when(mContentProvider.getType(contentUri)).thenReturn("image/jpeg");
        ShadowContentResolver.registerProviderInternal("com.example.provider", mContentProvider);
        assertEquals("image/jpeg", mHandler.getMimeType(mContext, contentUri));

        // File URI
        assertEquals(
                "text/html",
                mHandler.getMimeType(mContext, Uri.parse("file:///path/to/file.html")));
        assertEquals(
                "image/png", mHandler.getMimeType(mContext, Uri.parse("file:///path/to/file.PNG")));
        assertNull(mHandler.getMimeType(mContext, Uri.parse("file:///path/to/file.unknown")));
        assertNull(mHandler.getMimeType(mContext, Uri.parse("file:///path/to/file")));

        // HTTP URI (unsupported scheme)
        assertNull(mHandler.getMimeType(mContext, Uri.parse("http://example.com/file.html")));
    }

    @Test
    public void testHasContentUri() {
        ClipData.Item item1 = mock(ClipData.Item.class);
        ClipData.Item item2 = mock(ClipData.Item.class);

        when(mClipData.getItemCount()).thenReturn(2);
        when(mClipData.getItemAt(0)).thenReturn(item1);
        when(mClipData.getItemAt(1)).thenReturn(item2);

        // No content URIs
        when(item1.getUri()).thenReturn(Uri.parse("file:///path/to/file.txt"));
        when(item2.getUri()).thenReturn(null);
        assertFalse(mHandler.hasContentUri(mClipData));

        // One content URI
        when(item2.getUri()).thenReturn(Uri.parse("content://com.example/doc"));
        assertTrue(mHandler.hasContentUri(mClipData));
    }

    @Test
    public void testGetFallbackMimeType() {
        when(mClipDescription.getMimeTypeCount()).thenReturn(2);
        when(mClipDescription.getMimeType(0)).thenReturn("text/uri-list");
        when(mClipDescription.getMimeType(1)).thenReturn("image/png");

        assertEquals("image/png", mHandler.getFallbackMimeType(mClipDescription));

        // No acceptable types
        when(mClipDescription.getMimeType(1)).thenReturn("video/mp4");
        assertNull(mHandler.getFallbackMimeType(mClipDescription));
    }

    @Test
    public void testFindUriToLoad() {
        ClipData.Item item1 = mock(ClipData.Item.class);
        ClipData.Item item2 = mock(ClipData.Item.class);

        when(mClipData.getItemCount()).thenReturn(2);
        when(mClipData.getItemAt(0)).thenReturn(item1);
        when(mClipData.getItemAt(1)).thenReturn(item2);

        // Item 1 is unacceptable, Item 2 is acceptable file
        Uri fileUri1 = Uri.parse("file:///path/to/video.mp4");
        Uri fileUri2 = Uri.parse("file:///path/to/image.png");
        when(item1.getUri()).thenReturn(fileUri1);
        when(item2.getUri()).thenReturn(fileUri2);

        assertEquals(fileUri2, mHandler.findUriToLoad(mContext, mClipData, mClipDescription));

        // Single item, getMimeType returns null, fallback to desc
        when(mClipData.getItemCount()).thenReturn(1);
        Uri contentUri = Uri.parse("content://com.example/doc");
        when(item1.getUri()).thenReturn(contentUri);
        when(mContentProvider.getType(contentUri)).thenReturn(null);
        ShadowContentResolver.registerProviderInternal("com.example", mContentProvider);

        when(mClipDescription.getMimeTypeCount()).thenReturn(2);
        when(mClipDescription.getMimeType(0)).thenReturn("text/uri-list");
        when(mClipDescription.getMimeType(1)).thenReturn("application/pdf");

        assertEquals(contentUri, mHandler.findUriToLoad(mContext, mClipData, mClipDescription));
    }

    @Test
    public void testOnDrag_Drop_BrowsableIntent() {

        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(mDragEvent.getClipData()).thenReturn(mClipData);
        when(mClipData.getItemCount()).thenReturn(1);
        when(mClipData.getItemAt(0)).thenReturn(mClipDataItem);

        when(mClipDataItem.getUri()).thenReturn(null);
        when(mClipDataItem.getIntent()).thenReturn(mIntent);
        when(mIntent.hasCategory(Intent.CATEGORY_BROWSABLE)).thenReturn(true);
        Uri webUri = Uri.parse("https://www.example.com");
        when(mIntent.getData()).thenReturn(webUri);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, mDragEvent));

        verify(mTab, never()).addObserver(any());
        verify(mOmniboxStub).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals("https://www.example.com/", mLoadUrlParamsCaptor.getValue().url);
    }

    @Test
    public void testOnDrag_Drop_UnsupportedBrowsableIntent() {
        Intent intent =
                new Intent(Intent.ACTION_VIEW, Uri.parse("mailto:user@example.com"))
                        .addCategory(Intent.CATEGORY_BROWSABLE);
        ClipData clipData =
                new ClipData(
                        "url",
                        new String[] {ClipDescription.MIMETYPE_TEXT_URILIST},
                        new ClipData.Item(intent));

        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(mDragEvent.getClipData()).thenReturn(clipData);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        assertFalse(mHandler.onDrag(new View(mContext), mDragEvent));
        verify(mOmniboxStub, never()).loadUrl(any());
    }

    @Test
    public void testOnDrag_Drop_SkipsUnsupportedBrowsableIntent() {
        Intent mailIntent =
                new Intent(Intent.ACTION_VIEW, Uri.parse("mailto:user@example.com"))
                        .addCategory(Intent.CATEGORY_BROWSABLE);
        Uri webUri = Uri.parse("https://www.example.com");
        Intent webIntent =
                new Intent(Intent.ACTION_VIEW, webUri).addCategory(Intent.CATEGORY_BROWSABLE);
        ClipData clipData =
                new ClipData(
                        "urls",
                        new String[] {ClipDescription.MIMETYPE_TEXT_URILIST},
                        new ClipData.Item(mailIntent));
        clipData.addItem(new ClipData.Item(webIntent));

        when(mDragEvent.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(mDragEvent.getClipData()).thenReturn(clipData);
        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(mActivity));

        assertTrue(mHandler.onDrag(new View(mContext), mDragEvent));
        verify(mOmniboxStub).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals("https://www.example.com/", mLoadUrlParamsCaptor.getValue().url);
    }

    @Test
    public void testFindUriToLoad_AcceptsHttpBrowsableIntent() {
        Uri webUri = Uri.parse("http://www.example.com");
        Intent intent =
                new Intent(Intent.ACTION_VIEW, webUri).addCategory(Intent.CATEGORY_BROWSABLE);
        ClipData clipData =
                new ClipData(
                        "url",
                        new String[] {ClipDescription.MIMETYPE_TEXT_URILIST},
                        new ClipData.Item(intent));

        assertEquals(
                Uri.parse("http://www.example.com/"),
                mHandler.findUriToLoad(mContext, clipData, null));
    }

    @Test
    public void testFindUriToLoad_RejectsInvalidHttpBrowsableIntent() {
        Intent intent =
                new Intent(Intent.ACTION_VIEW, Uri.parse("http://"))
                        .addCategory(Intent.CATEGORY_BROWSABLE);
        ClipData clipData =
                new ClipData(
                        "url",
                        new String[] {ClipDescription.MIMETYPE_TEXT_URILIST},
                        new ClipData.Item(intent));

        assertNull(mHandler.findUriToLoad(mContext, clipData, null));
    }
}
