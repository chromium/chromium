// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.clipboard;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.ContextThemeWrapper;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.styles.OmniboxDrawableState;
import org.chromium.chrome.browser.omnibox.styles.OmniboxImageSupplier;
import org.chromium.chrome.browser.omnibox.styles.SuggestionSpannable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionCommonProperties;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionViewViewBinder;
import org.chromium.chrome.browser.omnibox.test.R;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.OmniboxSuggestionType;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.io.ByteArrayOutputStream;
import java.util.Optional;

/** Tests for {@link ClipboardSuggestionProcessor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ClipboardSuggestionProcessorUnitTest {
    private static final GURL TEST_URL = JUnitTestGURLs.EXAMPLE_URL;

    public @Rule MockitoRule mMockitoRule = MockitoJUnit.rule();

    private @Mock SuggestionHost mSuggestionHost;
    private @Mock OmniboxImageSupplier mImageSupplier;
    private @Mock Resources mResources;

    private Context mContext;
    private ClipboardSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;
    private Bitmap mBitmap;
    private ViewGroup mRootView;
    private TextView mTitleTextView;
    private TextView mContentTextView;
    private int mLastSetTextDirection = -1;

    @Before
    public void setUp() {
        mContext =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        mBitmap = Bitmap.createBitmap(10, 5, Bitmap.Config.ARGB_8888);
        mProcessor =
                new ClipboardSuggestionProcessor(
                        mContext, mSuggestionHost, Optional.of(mImageSupplier));
        mRootView = new LinearLayout(mContext);
        mTitleTextView = new TextView(mContext);
        mTitleTextView.setId(R.id.line_1);
        mContentTextView =
                new TextView(mContext) {
                    @Override
                    public void setTextDirection(int textDirection) {
                        super.setTextDirection(textDirection);
                        mLastSetTextDirection = textDirection;
                    }
                };
        mContentTextView.setId(R.id.line_2);
        mRootView.addView(mTitleTextView);
        mRootView.addView(mContentTextView);
    }

    /** Create clipboard suggestion for test, and click the reveal button. */
    private void createClipboardSuggestionAndClickReveal(int type, GURL url) {
        createClipboardSuggestion(type, url, null);

        // Click reveal button
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
    }

    /** Create clipboard suggestion for test, and click the reveal button. */
    private void createClipboardSuggestionAndClickReveal(
            int type, GURL url, byte[] clipboardImageData) {
        createClipboardSuggestion(type, url, clipboardImageData);

        // Click reveal button
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
    }

    /** Create clipboard suggestion for test. */
    private void createClipboardSuggestion(int type, GURL url) {
        createClipboardSuggestion(type, url, null);
    }

    /** Create clipboard suggestion for test. */
    private void createClipboardSuggestion(int type, GURL url, byte[] clipboardImageData) {
        mSuggestion =
                AutocompleteMatchBuilder.searchWithType(type)
                        .setIsSearch(type != OmniboxSuggestionType.CLIPBOARD_URL)
                        .setUrl(url)
                        .setClipboardImageData(clipboardImageData)
                        .build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, mModel, 0);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionViewProperties.TEXT_LINE_1_TEXT);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionCommonProperties.COLOR_SCHEME);
        SuggestionViewViewBinder.bind(
                mModel, mRootView, SuggestionViewProperties.IS_SEARCH_SUGGESTION);
        SuggestionViewViewBinder.bind(mModel, mRootView, SuggestionViewProperties.TEXT_LINE_2_TEXT);
    }

    @Test
    @SmallTest
    public void clipboardSuggestion_identifyUrlSuggestion() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        Assert.assertTrue(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
    }

    @Test
    @SmallTest
    public void clipboardSuggestion_showsFaviconWhenAvailable() {
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        createClipboardSuggestionAndClickReveal(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        OmniboxDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mImageSupplier).fetchFavicon(eq(TEST_URL), callback.capture());
        callback.getValue().onResult(mBitmap);
        OmniboxDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    @SmallTest
    public void clipboardSuggestion_showsFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        createClipboardSuggestionAndClickReveal(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        OmniboxDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mImageSupplier).fetchFavicon(eq(TEST_URL), callback.capture());
        callback.getValue().onResult(null);
        OmniboxDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }

    @Test
    @SmallTest
    public void clipobardSuggestion_urlAndTextDirection() {
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        // URL
        createClipboardSuggestionAndClickReveal(OmniboxSuggestionType.CLIPBOARD_URL, TEST_URL);
        Assert.assertFalse(mModel.get(SuggestionViewProperties.IS_SEARCH_SUGGESTION));
        verify(mImageSupplier).fetchFavicon(eq(TEST_URL), callback.capture());
        callback.getValue().onResult(null);
        Assert.assertEquals(TextView.TEXT_DIRECTION_LTR, mLastSetTextDirection);

        // Text
        createClipboardSuggestionAndClickReveal(
                OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertEquals(TextView.TEXT_DIRECTION_INHERIT, mLastSetTextDirection);
    }

    @Test
    @SmallTest
    public void clipboardSuggestion_showsThumbnailWhenAvailable() {
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        Assert.assertTrue(mBitmap.compress(Bitmap.CompressFormat.PNG, 100, baos));
        byte[] bitmapData = baos.toByteArray();
        createClipboardSuggestionAndClickReveal(
                OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL(), bitmapData);
        OmniboxDrawableState icon = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon);

        // Since |icon| is Bitmap -> PNG -> Bitmap, the image changed, we just check the size to
        // make sure they are same.
        Assert.assertEquals(
                mBitmap.getWidth(), ((BitmapDrawable) icon.drawable).getBitmap().getWidth());
        Assert.assertEquals(
                mBitmap.getHeight(), ((BitmapDrawable) icon.drawable).getBitmap().getHeight());
    }

    @Test
    @SmallTest
    public void clipboardSuggestion_thumbnailShouldResizeIfTooLarge() {
        int size =
                mContext.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_decoration_image_size);

        Bitmap largeBitmap = Bitmap.createBitmap(size * 2, size * 2, Bitmap.Config.ARGB_8888);
        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        Assert.assertTrue(largeBitmap.compress(Bitmap.CompressFormat.PNG, 100, baos));
        byte[] bitmapData = baos.toByteArray();
        createClipboardSuggestionAndClickReveal(
                OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL(), bitmapData);
        OmniboxDrawableState icon = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon);

        Assert.assertEquals(size, ((BitmapDrawable) icon.drawable).getBitmap().getWidth());
        Assert.assertEquals(size, ((BitmapDrawable) icon.drawable).getBitmap().getHeight());
    }

    @Test
    @SmallTest
    public void clipboardSuggestion_revealButton() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        Assert.assertNotNull(mModel.get(BaseSuggestionViewProperties.ACTION_BUTTONS));
    }

    @Test
    @SmallTest
    public void clipboardSuggestion_noContentByDefault() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        SuggestionSpannable textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());
    }

    @Test
    @SmallTest
    public void clipboardSuggestion_revealAndConcealButton() {
        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_URL, GURL.emptyGURL());
        SuggestionSpannable textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        // Click reveal button
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertNotEquals(0, textLine2.length());

        // Click conceal button
        mProcessor.concealButtonClickHandler(mSuggestion, mModel);
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_TEXT, GURL.emptyGURL());
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        // Click reveal button
        mProcessor.revealButtonClickHandler(mSuggestion, mModel);
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertNotEquals(0, textLine2.length());

        // Click conceal button
        mProcessor.concealButtonClickHandler(mSuggestion, mModel);
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());

        createClipboardSuggestion(OmniboxSuggestionType.CLIPBOARD_IMAGE, GURL.emptyGURL());
        textLine2 = mModel.get(SuggestionViewProperties.TEXT_LINE_2_TEXT);
        Assert.assertEquals(0, textLine2.length());
        // Image suggestions never have content in the text line 2.
    }
}
