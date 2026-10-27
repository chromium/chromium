// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.lenient;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.robolectric.Shadows.shadowOf;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.speech.RecognizerIntent;

import androidx.test.core.app.ApplicationProvider;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.gsa.GSAUtils;
import org.chromium.chrome.browser.omnibox.voice.VoicePlateIntentFactory.VoicePlateColorScheme;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionIntentHandler.VoiceInteractionSource;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.externalauth.ExternalAuthUtils;
import org.chromium.components.omnibox.OmniboxFeatureList;
import org.chromium.components.search_engines.TemplateUrlService;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.signin.test.util.TestAccounts;
import org.chromium.ui.util.ColorUtils;

/** Unit tests for {@link VoicePlateIntentFactory}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures(OmniboxFeatureList.OMNIBOX_VOICE_PLATE_INTENT)
public class VoicePlateIntentFactoryUnitTest {
    private static final ComponentName VOICE_PLATE_COMPONENT =
            new ComponentName(
                    GSAUtils.GSA_PACKAGE_NAME, VoicePlateIntentFactory.VOICE_PLATE_CLASS_NAME);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Profile mProfile;
    @Mock private TemplateUrlService mTemplateUrlService;
    @Mock private IdentityManager mIdentityManager;
    @Mock private ExternalAuthUtils mExternalAuthUtils;

    private final VoicePlateIntentFactory mFactory = new VoicePlateIntentFactory();
    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        IdentityServicesProvider.setIdentityManagerForTesting(mIdentityManager);
        GSAUtils.setFakePassableGsaEnvironmentForTesting(true);
        ShadowPackageManager packageManager = shadowOf(mContext.getPackageManager());
        packageManager.addActivityIfNotPresent(VOICE_PLATE_COMPONENT);
        // Robolectric, unlike Android, requires a matching filter to resolve explicit intents.
        packageManager.addIntentFilterForActivity(
                VOICE_PLATE_COMPONENT, new IntentFilter(RecognizerIntent.ACTION_RECOGNIZE_SPEECH));

        lenient().doReturn(true).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        lenient().doReturn(TestAccounts.ACCOUNT1).when(mIdentityManager).getPrimaryAccountInfo();
    }

    private @VoicePlateColorScheme int getColorScheme(Intent intent) {
        return intent.getIntExtra(
                VoicePlateIntentFactory.EXTRA_COLOR_SCHEME, /* defaultValue= */ 0);
    }

    @Test
    public void createIntent_eligible_buildsExplicitVoicePlateIntent() {
        Intent intent = mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX);

        assertNotNull(intent);
        assertEquals(RecognizerIntent.ACTION_RECOGNIZE_SPEECH, intent.getAction());
        assertEquals(VOICE_PLATE_COMPONENT, intent.getComponent());
        assertTrue(
                intent.getBooleanExtra(
                        VoicePlateIntentFactory.EXTRA_DISABLE_STOP_BUTTON,
                        /* defaultValue= */ false));
    }

    @Test
    public void createIntent_signedIn_passesAccountName() {
        Intent intent = mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX);

        assertNotNull(intent);
        assertEquals(
                TestAccounts.ACCOUNT1.getEmail(),
                intent.getStringExtra(VoicePlateIntentFactory.EXTRA_ACCOUNT_NAME));
        assertFalse(intent.hasExtra(VoicePlateIntentFactory.EXTRA_SIGNED_OUT));
    }

    @Test
    public void createIntent_signedOut_passesSignedOut() {
        doReturn(null).when(mIdentityManager).getPrimaryAccountInfo();

        Intent intent = mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX);

        assertNotNull(intent);
        assertTrue(
                intent.getBooleanExtra(
                        VoicePlateIntentFactory.EXTRA_SIGNED_OUT, /* defaultValue= */ false));
        assertFalse(intent.hasExtra(VoicePlateIntentFactory.EXTRA_ACCOUNT_NAME));
    }

    @Test
    public void createIntent_colorSchemeFollowsNightMode() {
        ColorUtils.setInNightModeForTesting(false);
        Intent lightIntent =
                mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX);
        ColorUtils.setInNightModeForTesting(true);
        Intent darkIntent =
                mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX);

        assertEquals(VoicePlateColorScheme.LIGHT, getColorScheme(lightIntent));
        assertEquals(VoicePlateColorScheme.DARK, getColorScheme(darkIntent));
    }

    @Test
    @DisableFeatures(OmniboxFeatureList.OMNIBOX_VOICE_PLATE_INTENT)
    public void createIntent_featureDisabled_returnsNull() {
        assertNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX));
    }

    @Test
    public void createIntent_supportedSources_returnsIntent() {
        assertNotNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX));
        assertNotNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.NTP));
        assertNotNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.TOOLBAR));
    }

    @Test
    public void createIntent_unsupportedSources_returnsNull() {
        assertNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.SEARCH_WIDGET));
        assertNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.COMPOSEBOX));
        assertNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.TASKS_SURFACE));
    }

    @Test
    public void createIntent_noProfile_returnsNull() {
        assertNull(
                mFactory.createIntent(
                        mContext, /* profile= */ null, VoiceInteractionSource.OMNIBOX));
    }

    @Test
    public void createIntent_incognito_returnsNull() {
        doReturn(true).when(mProfile).isOffTheRecord();
        assertNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX));
    }

    @Test
    public void createIntent_nonGoogleSearchEngine_returnsNull() {
        doReturn(false).when(mTemplateUrlService).isDefaultSearchEngineGoogle();
        assertNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX));
    }

    @Test
    public void createIntent_invalidAgsaPackage_skipsAccountLookup() {
        GSAUtils.setFakePassableGsaEnvironmentForTesting(false);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);

        assertNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX));
        verify(mExternalAuthUtils).isGoogleSigned(GSAUtils.GSA_PACKAGE_NAME);
        verify(mIdentityManager, never()).getPrimaryAccountInfo();
    }

    @Test
    public void createIntent_voicePlateNotResolvable_skipsSignatureAndAccountChecks() {
        GSAUtils.setFakePassableGsaEnvironmentForTesting(false);
        ExternalAuthUtils.setInstanceForTesting(mExternalAuthUtils);
        shadowOf(mContext.getPackageManager()).removeActivity(VOICE_PLATE_COMPONENT);

        assertNull(mFactory.createIntent(mContext, mProfile, VoiceInteractionSource.OMNIBOX));
        verify(mExternalAuthUtils, never()).isGoogleSigned(any());
        verify(mIdentityManager, never()).getPrimaryAccountInfo();
    }
}
