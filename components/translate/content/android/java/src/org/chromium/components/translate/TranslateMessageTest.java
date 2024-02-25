// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.translate;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import android.app.Activity;
import android.content.Context;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;

import org.chromium.base.UnownedUserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageBannerProperties;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.components.messages.PrimaryActionClickBehavior;
import org.chromium.components.messages.PrimaryWidgetAppearance;
import org.chromium.components.messages.SecondaryMenuMaxSize;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.ref.WeakReference;

/** Unit tests for TranslateMessage. */
@RunWith(BaseRobolectricTestRunner.class)
public final class TranslateMessageTest {
    private static final long NATIVE_TRANSLATE_MESSAGE = 1337;
    private static final int DISMISSAL_DURATION_SECONDS = 15;

    private static final String TITLE_BEFORE_TRANSLATE = "Translate Page?";
    private static final String TITLE_AFTER_TRANSLATE = "Page Translated";
    private static final String PRIMARY_TEXT_TRANSLATE = "Translate";
    private static final String PRIMARY_TEXT_UNDO = "Undo";
    private static final String DESCRIPTION = "French to English";

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock WebContents mWebContents;
    @Mock TranslateMessage.Natives mMockJni;
    @Mock Context mContext;
    @Mock MessageDispatcher mMessageDispatcher;

    @Captor ArgumentCaptor<PropertyModel> mPropertyModelCaptor;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mJniMocker.mock(TranslateMessageJni.TEST_HOOKS, mMockJni);
    }

    @Test
    @SmallTest
    public void testCreateWithNullWindowAndroid() {
        doReturn(null).when(mWebContents).getTopLevelNativeWindow();
        Assert.assertNull(
                TranslateMessage.create(
                        mWebContents, NATIVE_TRANSLATE_MESSAGE, DISMISSAL_DURATION_SECONDS));
    }

    @Test
    @SmallTest
    public void testCreateWithNullActivityWeakReference() {
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        doReturn(null).when(windowAndroid).getActivity();
        doReturn(windowAndroid).when(mWebContents).getTopLevelNativeWindow();
        Assert.assertNull(
                TranslateMessage.create(
                        mWebContents, NATIVE_TRANSLATE_MESSAGE, DISMISSAL_DURATION_SECONDS));
    }

    @Test
    @SmallTest
    public void testCreateWithNullActivity() {
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);
        doReturn(new WeakReference<Activity>(null)).when(windowAndroid).getActivity();
        doReturn(windowAndroid).when(mWebContents).getTopLevelNativeWindow();
        Assert.assertNull(
                TranslateMessage.create(
                        mWebContents, NATIVE_TRANSLATE_MESSAGE, DISMISSAL_DURATION_SECONDS));
    }

    @Test
    @SmallTest
    public void testCreateWithNullMessageDispatcher() {
        Activity activity = Mockito.mock(Activity.class);
        WindowAndroid windowAndroid = Mockito.mock(WindowAndroid.class);

        doReturn(new WeakReference<Activity>(activity)).when(windowAndroid).getActivity();
        doReturn(windowAndroid).when(mWebContents).getTopLevelNativeWindow();
        doReturn(new UnownedUserDataHost()).when(windowAndroid).getUnownedUserDataHost();

        Assert.assertNull(
                TranslateMessage.create(
                        mWebContents, NATIVE_TRANSLATE_MESSAGE, DISMISSAL_DURATION_SECONDS));
    }

    @Test
    @SmallTest
    public void testFullTranslateFlowThenDismissViaGesture() {
        TranslateMessage translateMessage =
                new TranslateMessage(
                        mContext,
                        mMessageDispatcher,
                        mWebContents,
                        NATIVE_TRANSLATE_MESSAGE,
                        DISMISSAL_DURATION_SECONDS);

        // Show the before translate message.
        translateMessage.showMessage(
                TITLE_BEFORE_TRANSLATE,
                DESCRIPTION,
                PRIMARY_TEXT_TRANSLATE,
                /* hasOverflowMenu= */ true);

        verify(mMessageDispatcher)
                .enqueueMessage(
                        mPropertyModelCaptor.capture(),
                        eq(mWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        /* highPriority= */ eq(false));
        PropertyModel messageProperties = mPropertyModelCaptor.getValue();

        assertHasCommonProperties(messageProperties);
        assertHasOverflowMenuProperties(messageProperties);
        Assert.assertEquals(
                TITLE_BEFORE_TRANSLATE, messageProperties.get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                DESCRIPTION, messageProperties.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET,
                messageProperties.get(MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE));
        Assert.assertEquals(
                PRIMARY_TEXT_TRANSLATE,
                messageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));

        // Show the translation-in-progress state upon clicking "Translate".
        doAnswer(
                        new Answer() {
                            @Override
                            public Object answer(InvocationOnMock invocation) {
                                translateMessage.showMessage(
                                        TITLE_BEFORE_TRANSLATE,
                                        DESCRIPTION,
                                        "",
                                        /* hasOverflowMenu= */ true);
                                return null;
                            }
                        })
                .when(mMockJni)
                .handlePrimaryAction(NATIVE_TRANSLATE_MESSAGE);

        Assert.assertEquals(
                Integer.valueOf(PrimaryActionClickBehavior.DO_NOT_DISMISS),
                messageProperties.get(MessageBannerProperties.ON_PRIMARY_ACTION).get());

        verifyNoMoreInteractions(mMessageDispatcher);

        assertHasCommonProperties(messageProperties);
        assertHasOverflowMenuProperties(messageProperties);
        Assert.assertEquals(
                TITLE_BEFORE_TRANSLATE, messageProperties.get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                DESCRIPTION, messageProperties.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                PrimaryWidgetAppearance.PROGRESS_SPINNER,
                messageProperties.get(MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE));

        // Show the after translate message.
        translateMessage.showMessage(
                TITLE_AFTER_TRANSLATE, DESCRIPTION, PRIMARY_TEXT_UNDO, /* hasOverflowMenu= */ true);

        verifyNoMoreInteractions(mMessageDispatcher);

        assertHasCommonProperties(messageProperties);
        assertHasOverflowMenuProperties(messageProperties);
        Assert.assertEquals(
                TITLE_AFTER_TRANSLATE, messageProperties.get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                DESCRIPTION, messageProperties.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET,
                messageProperties.get(MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE));
        Assert.assertEquals(
                PRIMARY_TEXT_UNDO,
                messageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));

        messageProperties.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.GESTURE);
        verify(mMockJni).handleDismiss(NATIVE_TRANSLATE_MESSAGE, DismissReason.GESTURE);
    }

    @Test
    @SmallTest
    public void testShowMessageWithoutOverflowMenu() {
        TranslateMessage translateMessage =
                new TranslateMessage(
                        mContext,
                        mMessageDispatcher,
                        mWebContents,
                        NATIVE_TRANSLATE_MESSAGE,
                        DISMISSAL_DURATION_SECONDS);

        // Show the message.
        translateMessage.showMessage(
                TITLE_BEFORE_TRANSLATE,
                DESCRIPTION,
                PRIMARY_TEXT_TRANSLATE,
                /* hasOverflowMenu= */ false);

        verify(mMessageDispatcher)
                .enqueueMessage(
                        mPropertyModelCaptor.capture(),
                        eq(mWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        /* highPriority= */ eq(false));
        PropertyModel messageProperties = mPropertyModelCaptor.getValue();

        assertHasCommonProperties(messageProperties);
        Assert.assertFalse(
                messageProperties
                        .getAllSetProperties()
                        .contains(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID));
        Assert.assertEquals(
                TITLE_BEFORE_TRANSLATE, messageProperties.get(MessageBannerProperties.TITLE));
        Assert.assertEquals(
                DESCRIPTION, messageProperties.get(MessageBannerProperties.DESCRIPTION));
        Assert.assertEquals(
                PrimaryWidgetAppearance.BUTTON_IF_TEXT_IS_SET,
                messageProperties.get(MessageBannerProperties.PRIMARY_WIDGET_APPEARANCE));
        Assert.assertEquals(
                PRIMARY_TEXT_TRANSLATE,
                messageProperties.get(MessageBannerProperties.PRIMARY_BUTTON_TEXT));
    }

    @Test
    @SmallTest
    public void testDismissFromNative() {
        TranslateMessage translateMessage =
                new TranslateMessage(
                        mContext,
                        mMessageDispatcher,
                        mWebContents,
                        NATIVE_TRANSLATE_MESSAGE,
                        DISMISSAL_DURATION_SECONDS);
        translateMessage.showMessage(
                TITLE_BEFORE_TRANSLATE,
                DESCRIPTION,
                PRIMARY_TEXT_TRANSLATE,
                /* hasOverflowMenu= */ true);

        verify(mMessageDispatcher)
                .enqueueMessage(
                        mPropertyModelCaptor.capture(),
                        eq(mWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        /* highPriority= */ eq(false));
        PropertyModel messageProperties = mPropertyModelCaptor.getValue();

        // Call the ON_DISMISSED callback from MessageDispatcher.dismissMessage.
        doAnswer(
                        new Answer() {
                            @Override
                            public Object answer(InvocationOnMock invocation) {
                                messageProperties
                                        .get(MessageBannerProperties.ON_DISMISSED)
                                        .onResult(DismissReason.DISMISSED_BY_FEATURE);
                                return null;
                            }
                        })
                .when(mMessageDispatcher)
                .dismissMessage(messageProperties, DismissReason.DISMISSED_BY_FEATURE);

        translateMessage.dismiss();

        verify(mMockJni)
                .handleDismiss(NATIVE_TRANSLATE_MESSAGE, DismissReason.DISMISSED_BY_FEATURE);
    }

    @Test
    @SmallTest
    public void testClearNativePointer() {
        TranslateMessage translateMessage =
                new TranslateMessage(
                        mContext,
                        mMessageDispatcher,
                        mWebContents,
                        NATIVE_TRANSLATE_MESSAGE,
                        DISMISSAL_DURATION_SECONDS);
        translateMessage.showMessage(
                TITLE_BEFORE_TRANSLATE,
                DESCRIPTION,
                PRIMARY_TEXT_TRANSLATE,
                /* hasOverflowMenu= */ true);

        verify(mMessageDispatcher)
                .enqueueMessage(
                        mPropertyModelCaptor.capture(),
                        eq(mWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        /* highPriority= */ eq(false));
        PropertyModel messageProperties = mPropertyModelCaptor.getValue();

        translateMessage.clearNativePointer();

        // No native methods should be called after clearing the native pointer.
        messageProperties.get(MessageBannerProperties.ON_PRIMARY_ACTION).get();
        messageProperties.get(MessageBannerProperties.ON_DISMISSED).onResult(DismissReason.GESTURE);
        Assert.assertNull(
                translateMessage.handleSecondaryMenuItemClicked(
                        new TranslateMessage.MenuItem("More languages", "", false, 2, "")));

        verifyNoMoreInteractions(mMockJni);
    }

    @Test
    @SmallTest
    public void testConstructMenuItemArray() {
        final String[] titles = new String[] {"foo", "bar", "", "English", "French"};
        final String[] subtitles = new String[] {"", "", "", "", "Français"};
        final boolean[] hasCheckmarks = new boolean[] {false, true, false, false, false};
        final int[] overflowMenuItemIds = new int[] {0, 1, 2, 3, 4};
        final String[] languageCodes = new String[] {"", "", "", "en", "fr"};

        TranslateMessage.MenuItem[] menuItems =
                TranslateMessage.constructMenuItemArray(
                        titles, subtitles, hasCheckmarks, overflowMenuItemIds, languageCodes);
        Assert.assertEquals(titles.length, menuItems.length);
        for (int i = 0; i < menuItems.length; ++i) {
            Assert.assertEquals(titles[i], menuItems[i].title);
            Assert.assertEquals(subtitles[i], menuItems[i].subtitle);
            Assert.assertEquals(hasCheckmarks[i], menuItems[i].hasCheckmark);
            Assert.assertEquals(overflowMenuItemIds[i], menuItems[i].overflowMenuItemId);
            Assert.assertEquals(languageCodes[i], menuItems[i].languageCode);
        }
    }

    private static void assertHasCommonProperties(PropertyModel messageProperties) {
        Assert.assertEquals(
                MessageIdentifier.TRANSLATE,
                messageProperties.get(MessageBannerProperties.MESSAGE_IDENTIFIER));
        Assert.assertEquals(
                R.drawable.infobar_translate_compact,
                messageProperties.get(MessageBannerProperties.ICON_RESOURCE_ID));
        Assert.assertEquals(
                MessageBannerProperties.TINT_NONE,
                messageProperties.get(MessageBannerProperties.ICON_TINT_COLOR));
        Assert.assertEquals(
                DISMISSAL_DURATION_SECONDS,
                messageProperties.get(MessageBannerProperties.DISMISSAL_DURATION));
        Assert.assertNotNull(messageProperties.get(MessageBannerProperties.ON_PRIMARY_ACTION));
        Assert.assertNotNull(messageProperties.get(MessageBannerProperties.ON_DISMISSED));
    }

    private static void assertHasOverflowMenuProperties(PropertyModel messageProperties) {
        Assert.assertEquals(
                R.drawable.settings_cog,
                messageProperties.get(MessageBannerProperties.SECONDARY_ICON_RESOURCE_ID));
        Assert.assertNotNull(
                messageProperties.get(MessageBannerProperties.SECONDARY_MENU_BUTTON_DELEGATE));
        Assert.assertEquals(
                SecondaryMenuMaxSize.LARGE,
                messageProperties.get(MessageBannerProperties.SECONDARY_MENU_MAX_SIZE));
        Assert.assertEquals(
                MessageIdentifier.TRANSLATE,
                messageProperties.get(MessageBannerProperties.MESSAGE_IDENTIFIER));
    }
}
