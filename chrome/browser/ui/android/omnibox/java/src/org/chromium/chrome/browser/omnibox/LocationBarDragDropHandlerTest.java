// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
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
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
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
@Config(manifest = Config.NONE)
public class LocationBarDragDropHandlerTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private OmniboxStub mOmniboxStub;
    @Mock private LocationBarDataProvider mDataProvider;
    @Mock private Tab mTab;
    @Mock private WindowAndroid mWindowAndroid;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;
    @Captor private ArgumentCaptor<OmniboxLoadUrlParams> mLoadUrlParamsCaptor;

    private LocationBarDragDropHandler mHandler;
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        when(mDataProvider.getTab()).thenReturn(mTab);
        when(mTab.getWindowAndroid()).thenReturn(mWindowAndroid);
        mHandler = new LocationBarDragDropHandler(mOmniboxStub, mDataProvider);

        ShadowMimeTypeMap shadowMimeTypeMap = Shadows.shadowOf(MimeTypeMap.getSingleton());
        shadowMimeTypeMap.addExtensionMimeTypeMapping("html", "text/html");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("png", "image/png");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("txt", "text/plain");
        shadowMimeTypeMap.addExtensionMimeTypeMapping("pdf", "application/pdf");
    }

    @Test
    public void testOnDrag_DragStarted_ValidMimeType() {
        DragEvent event = mock(DragEvent.class);
        ClipDescription desc = mock(ClipDescription.class);
        when(event.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(event.getClipDescription()).thenReturn(desc);
        when(desc.getMimeTypeCount()).thenReturn(1);
        when(desc.getMimeType(0)).thenReturn("text/plain");

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, event));
    }

    @Test
    public void testOnDrag_DragStarted_ChromeInternalMimeType() {
        DragEvent event = mock(DragEvent.class);
        ClipDescription desc = mock(ClipDescription.class);
        when(event.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(event.getClipDescription()).thenReturn(desc);
        when(desc.getMimeTypeCount()).thenReturn(1);
        when(desc.getMimeType(0)).thenReturn("chrome/tab");

        View view = new View(mContext);
        assertFalse(mHandler.onDrag(view, event));
    }

    @Test
    public void testOnDrag_DragStarted_InvalidMimeType() {
        DragEvent event = mock(DragEvent.class);
        ClipDescription desc = mock(ClipDescription.class);
        when(event.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(event.getClipDescription()).thenReturn(desc);
        when(desc.getMimeTypeCount()).thenReturn(1);
        when(desc.getMimeType(0)).thenReturn("video/mp4");

        View view = new View(mContext);
        assertFalse(mHandler.onDrag(view, event));
    }

    @Test
    public void testOnDrag_DragStarted_TabDragWithUriList_Accepted() {
        DragEvent event = mock(DragEvent.class);
        ClipDescription desc = mock(ClipDescription.class);
        when(event.getAction()).thenReturn(DragEvent.ACTION_DRAG_STARTED);
        when(event.getClipDescription()).thenReturn(desc);
        when(desc.getMimeTypeCount()).thenReturn(2);
        when(desc.getMimeType(0)).thenReturn("chrome/tab");
        when(desc.getMimeType(1)).thenReturn("text/uri-list");
        when(desc.hasMimeType(ClipDescription.MIMETYPE_TEXT_URILIST)).thenReturn(true);

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, event));
    }

    @Test
    public void testOnDrag_Drop_FileUri() {
        DragEvent event = mock(DragEvent.class);
        ClipData clipData = mock(ClipData.class);
        ClipData.Item item = mock(ClipData.Item.class);
        Activity activity = mock(Activity.class);

        when(event.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(event.getClipData()).thenReturn(clipData);
        when(clipData.getItemCount()).thenReturn(1);
        when(clipData.getItemAt(0)).thenReturn(item);

        Uri fileUri = Uri.parse("file:///path/to/file.txt");
        when(item.getUri()).thenReturn(fileUri);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, event));

        verify(mTab, never()).addObserver(any());
        verify(mOmniboxStub).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals("file:///path/to/file.txt", mLoadUrlParamsCaptor.getValue().url);
    }

    @Test
    public void testOnDrag_Drop_ContentUri() {
        DragEvent event = mock(DragEvent.class);
        ClipData clipData = mock(ClipData.class);
        ClipData.Item item = mock(ClipData.Item.class);
        Activity activity = mock(Activity.class);
        DragAndDropPermissions permissions = mock(DragAndDropPermissions.class);

        when(event.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(event.getClipData()).thenReturn(clipData);
        when(clipData.getItemCount()).thenReturn(1);
        when(clipData.getItemAt(0)).thenReturn(item);

        Uri contentUri = Uri.parse("content://com.example.provider/document.pdf");
        when(item.getUri()).thenReturn(contentUri);

        ContentProvider mockProvider = mock(ContentProvider.class);
        when(mockProvider.getType(contentUri)).thenReturn("application/pdf");
        ShadowContentResolver.registerProviderInternal("com.example.provider", mockProvider);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
        when(activity.requestDragAndDropPermissions(event)).thenReturn(permissions);

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, event));

        verify(mTab).addObserver(mTabObserverCaptor.capture());
        verify(mOmniboxStub).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals(
                "content://com.example.provider/document.pdf", mLoadUrlParamsCaptor.getValue().url);

        verify(permissions, never()).release();

        mTabObserverCaptor.getValue().onPageLoadFinished(mTab, new GURL(contentUri.toString()));

        verify(permissions).release();
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testOnDrag_Drop_ContentUri_LoadFailed() {
        DragEvent event = mock(DragEvent.class);
        ClipData clipData = mock(ClipData.class);
        ClipData.Item item = mock(ClipData.Item.class);
        Activity activity = mock(Activity.class);
        DragAndDropPermissions permissions = mock(DragAndDropPermissions.class);

        when(event.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(event.getClipData()).thenReturn(clipData);
        when(clipData.getItemCount()).thenReturn(1);
        when(clipData.getItemAt(0)).thenReturn(item);

        Uri contentUri = Uri.parse("content://com.example.provider/document.pdf");
        when(item.getUri()).thenReturn(contentUri);

        ContentProvider mockProvider = mock(ContentProvider.class);
        when(mockProvider.getType(contentUri)).thenReturn("application/pdf");
        ShadowContentResolver.registerProviderInternal("com.example.provider", mockProvider);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
        when(activity.requestDragAndDropPermissions(event)).thenReturn(permissions);

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, event));

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        mTabObserverCaptor.getValue().onPageLoadFailed(mTab, 0);

        verify(permissions).release();
        verify(mTab).removeObserver(mTabObserverCaptor.getValue());
    }

    @Test
    public void testOnDrag_Drop_ContentUri_TabDestroyed() {
        DragEvent event = mock(DragEvent.class);
        ClipData clipData = mock(ClipData.class);
        ClipData.Item item = mock(ClipData.Item.class);
        Activity activity = mock(Activity.class);
        DragAndDropPermissions permissions = mock(DragAndDropPermissions.class);

        when(event.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(event.getClipData()).thenReturn(clipData);
        when(clipData.getItemCount()).thenReturn(1);
        when(clipData.getItemAt(0)).thenReturn(item);

        Uri contentUri = Uri.parse("content://com.example.provider/document.pdf");
        when(item.getUri()).thenReturn(contentUri);

        ContentProvider mockProvider = mock(ContentProvider.class);
        when(mockProvider.getType(contentUri)).thenReturn("application/pdf");
        ShadowContentResolver.registerProviderInternal("com.example.provider", mockProvider);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));
        when(activity.requestDragAndDropPermissions(event)).thenReturn(permissions);

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, event));

        verify(mTab).addObserver(mTabObserverCaptor.capture());

        mTabObserverCaptor.getValue().onDestroyed(mTab);

        verify(permissions).release();
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
        ContentProvider mockProvider = mock(ContentProvider.class);
        when(mockProvider.getType(contentUri)).thenReturn("image/jpeg");
        ShadowContentResolver.registerProviderInternal("com.example.provider", mockProvider);
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
        ClipData clipData = mock(ClipData.class);
        ClipData.Item item1 = mock(ClipData.Item.class);
        ClipData.Item item2 = mock(ClipData.Item.class);

        when(clipData.getItemCount()).thenReturn(2);
        when(clipData.getItemAt(0)).thenReturn(item1);
        when(clipData.getItemAt(1)).thenReturn(item2);

        // No content URIs
        when(item1.getUri()).thenReturn(Uri.parse("file:///path/to/file.txt"));
        when(item2.getUri()).thenReturn(null);
        assertFalse(mHandler.hasContentUri(clipData));

        // One content URI
        when(item2.getUri()).thenReturn(Uri.parse("content://com.example/doc"));
        assertTrue(mHandler.hasContentUri(clipData));
    }

    @Test
    public void testGetFallbackMimeType() {
        ClipDescription desc = mock(ClipDescription.class);
        when(desc.getMimeTypeCount()).thenReturn(2);
        when(desc.getMimeType(0)).thenReturn("text/uri-list");
        when(desc.getMimeType(1)).thenReturn("image/png");

        assertEquals("image/png", mHandler.getFallbackMimeType(desc));

        // No acceptable types
        when(desc.getMimeType(1)).thenReturn("video/mp4");
        assertNull(mHandler.getFallbackMimeType(desc));
    }

    @Test
    public void testFindUriToLoad() {
        ClipData clipData = mock(ClipData.class);
        ClipData.Item item1 = mock(ClipData.Item.class);
        ClipData.Item item2 = mock(ClipData.Item.class);
        ClipDescription desc = mock(ClipDescription.class);

        when(clipData.getItemCount()).thenReturn(2);
        when(clipData.getItemAt(0)).thenReturn(item1);
        when(clipData.getItemAt(1)).thenReturn(item2);

        // Item 1 is unacceptable, Item 2 is acceptable file
        Uri fileUri1 = Uri.parse("file:///path/to/video.mp4");
        Uri fileUri2 = Uri.parse("file:///path/to/image.png");
        when(item1.getUri()).thenReturn(fileUri1);
        when(item2.getUri()).thenReturn(fileUri2);

        assertEquals(fileUri2, mHandler.findUriToLoad(mContext, clipData, desc));

        // Single item, getMimeType returns null, fallback to desc
        when(clipData.getItemCount()).thenReturn(1);
        Uri contentUri = Uri.parse("content://com.example/doc");
        when(item1.getUri()).thenReturn(contentUri);
        ContentProvider mockProvider = mock(ContentProvider.class);
        when(mockProvider.getType(contentUri)).thenReturn(null);
        ShadowContentResolver.registerProviderInternal("com.example", mockProvider);

        when(desc.getMimeTypeCount()).thenReturn(2);
        when(desc.getMimeType(0)).thenReturn("text/uri-list");
        when(desc.getMimeType(1)).thenReturn("application/pdf");

        assertEquals(contentUri, mHandler.findUriToLoad(mContext, clipData, desc));
    }

    @Test
    public void testOnDrag_Drop_BrowsableIntent() {
        DragEvent event = mock(DragEvent.class);
        ClipData clipData = mock(ClipData.class);
        ClipData.Item item = mock(ClipData.Item.class);
        Activity activity = mock(Activity.class);
        Intent intent = mock(Intent.class);

        when(event.getAction()).thenReturn(DragEvent.ACTION_DROP);
        when(event.getClipData()).thenReturn(clipData);
        when(clipData.getItemCount()).thenReturn(1);
        when(clipData.getItemAt(0)).thenReturn(item);

        when(item.getUri()).thenReturn(null);
        when(item.getIntent()).thenReturn(intent);
        when(intent.hasCategory(Intent.CATEGORY_BROWSABLE)).thenReturn(true);
        Uri webUri = Uri.parse("https://www.example.com");
        when(intent.getData()).thenReturn(webUri);

        when(mWindowAndroid.getActivity()).thenReturn(new WeakReference<>(activity));

        View view = new View(mContext);
        assertTrue(mHandler.onDrag(view, event));

        verify(mTab, never()).addObserver(any());
        verify(mOmniboxStub).loadUrl(mLoadUrlParamsCaptor.capture());
        assertEquals("https://www.example.com", mLoadUrlParamsCaptor.getValue().url);
    }
}
