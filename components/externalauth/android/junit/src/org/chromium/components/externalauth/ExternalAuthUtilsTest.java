// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.externalauth;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import com.google.android.gms.common.ConnectionResult;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Robolectric tests for {@link ExternalAuthUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ExternalAuthUtilsTest {
    private static final int ERR = 999;
    private static final String SEARCH_APP_PACKAGE = "com.google.android.googlequicksearchbox";
    private static final String SYSTEM_BUILT_PACKAGE = "system.built.package.name";
    private static final String THIRD_PARTY_PACKAGE = "third.party.package.name";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private Context mContext;
    @Mock private ExternalAuthUtils mExternalAuthUtils;
    @Mock private UserRecoverableErrorHandler mUserRecoverableErrorHandler;

    @Mock private PackageManager mMockPackageManager;

    private String[] mMockCallingPackages;
    private String[] mAllMockPackages;
    private String[] mSystemOnlyMockPackages;

    @Before
    public void setUp() throws Exception {
        ContextUtils.initApplicationContextForTests(mContext);
        Mockito.lenient().when(mContext.getPackageManager()).thenReturn(mMockPackageManager);
        Mockito.lenient()
                .when(mMockPackageManager.getApplicationInfo(anyString(), anyInt()))
                .thenAnswer(
                        a -> {
                            String packageName = (String) a.getArguments()[0];
                            ApplicationInfo appInfo = new ApplicationInfo();
                            if (TextUtils.isEmpty(packageName)
                                    || packageName.equals(THIRD_PARTY_PACKAGE)) {
                                return appInfo;
                            }

                            appInfo.flags = ApplicationInfo.FLAG_SYSTEM;
                            return appInfo;
                        });
        mAllMockPackages =
                new String[] {SYSTEM_BUILT_PACKAGE, SEARCH_APP_PACKAGE, THIRD_PARTY_PACKAGE};
        mSystemOnlyMockPackages = new String[] {SYSTEM_BUILT_PACKAGE, SEARCH_APP_PACKAGE};
        mMockCallingPackages = mAllMockPackages;
    }

    @Test
    @Feature({"GooglePlayServices"})
    public void testCanUseGooglePlayServicesSuccess() {
        when(mExternalAuthUtils.canUseGooglePlayServices(any(UserRecoverableErrorHandler.class)))
                .thenCallRealMethod();
        when(mExternalAuthUtils.checkGooglePlayServicesAvailable(mContext))
                .thenReturn(ConnectionResult.SUCCESS);
        assertTrue(mExternalAuthUtils.canUseGooglePlayServices(mUserRecoverableErrorHandler));
        verifyNoMoreInteractions(mUserRecoverableErrorHandler);

        // Verifying stubs can be an anti-pattern but here it is important to
        // test that the real method canUseGooglePlayServices did invoke these
        // methods, which subclasses are expected to be able to override.
        InOrder inOrder = inOrder(mExternalAuthUtils);
        inOrder.verify(mExternalAuthUtils).checkGooglePlayServicesAvailable(mContext);
        inOrder.verify(mExternalAuthUtils, never()).isUserRecoverableError(anyInt());
        inOrder.verify(mExternalAuthUtils, never()).describeError(anyInt());
    }

    @Test
    @Feature({"GooglePlayServices"})
    public void testCanUseGooglePlayServicesNonUserRecoverableFailure() {
        when(mExternalAuthUtils.canUseGooglePlayServices(any(UserRecoverableErrorHandler.class)))
                .thenCallRealMethod();
        when(mExternalAuthUtils.checkGooglePlayServicesAvailable(mContext)).thenReturn(ERR);
        when(mExternalAuthUtils.isUserRecoverableError(ERR)).thenReturn(false); // Non-recoverable
        assertFalse(mExternalAuthUtils.canUseGooglePlayServices(mUserRecoverableErrorHandler));
        verifyNoMoreInteractions(mUserRecoverableErrorHandler);

        // Verifying stubs can be an anti-pattern but here it is important to
        // test that the real method canUseGooglePlayServices did invoke these
        // methods, which subclasses are expected to be able to override.
        InOrder inOrder = inOrder(mExternalAuthUtils);
        inOrder.verify(mExternalAuthUtils).checkGooglePlayServicesAvailable(mContext);
        inOrder.verify(mExternalAuthUtils).isUserRecoverableError(ERR);
    }

    @Test
    @Feature({"GooglePlayServices"})
    public void testCanUseGooglePlayServicesUserRecoverableFailure() {
        when(mExternalAuthUtils.canUseGooglePlayServices(any(UserRecoverableErrorHandler.class)))
                .thenCallRealMethod();
        doNothing().when(mUserRecoverableErrorHandler).handleError(mContext, ERR);
        when(mExternalAuthUtils.checkGooglePlayServicesAvailable(mContext)).thenReturn(ERR);
        when(mExternalAuthUtils.isUserRecoverableError(ERR)).thenReturn(true); // Recoverable
        when(mExternalAuthUtils.describeError(anyInt())).thenReturn("unused"); // For completeness
        assertFalse(mExternalAuthUtils.canUseGooglePlayServices(mUserRecoverableErrorHandler));

        // Verifying stubs can be an anti-pattern but here it is important to
        // test that the real method canUseGooglePlayServices did invoke these
        // methods, which subclasses are expected to be able to override.
        InOrder inOrder = inOrder(mExternalAuthUtils, mUserRecoverableErrorHandler);
        inOrder.verify(mExternalAuthUtils).checkGooglePlayServicesAvailable(mContext);
        inOrder.verify(mExternalAuthUtils).isUserRecoverableError(ERR);
        inOrder.verify(mUserRecoverableErrorHandler).handleError(mContext, ERR);
    }

    /** Test util for checking whether a package is system built. */
    @Test
    @Feature({"GooglePlayServices"})
    public void testSystemBuilt() {
        assertTrue(
                "System built check error",
                new ExternalAuthUtils().isSystemBuild(mMockPackageManager, SYSTEM_BUILT_PACKAGE));
        assertTrue(
                "System built check error",
                new ExternalAuthUtils().isSystemBuild(mMockPackageManager, SEARCH_APP_PACKAGE));
        assertFalse(
                "System built check error",
                new ExternalAuthUtils().isSystemBuild(mMockPackageManager, THIRD_PARTY_PACKAGE));
    }

    /** Test util checking caller validity against a given package name. */
    @Test
    @Feature({"GooglePlayServices"})
    public void testCheckCallerIsValidForPackage() {
        Mockito.when(mMockPackageManager.getPackagesForUid(anyInt()))
                .thenReturn(mMockCallingPackages, mMockCallingPackages, mSystemOnlyMockPackages);
        assertTrue(
                "Returned false when system built package was used",
                new ExternalAuthUtils()
                        .isCallerValidForPackage(
                                ExternalAuthUtils.FLAG_SHOULD_BE_SYSTEM, SYSTEM_BUILT_PACKAGE));
        assertFalse(
                "Returned true when package should not have been valid",
                new ExternalAuthUtils()
                        .isCallerValidForPackage(
                                ExternalAuthUtils.FLAG_SHOULD_BE_SYSTEM, THIRD_PARTY_PACKAGE));
        assertFalse(
                "Returned true when package should not have been found",
                new ExternalAuthUtils()
                        .isCallerValidForPackage(
                                ExternalAuthUtils.FLAG_SHOULD_BE_SYSTEM, THIRD_PARTY_PACKAGE));
    }

    /** Test util checking caller validity without a given package name. */
    @Test
    @Feature({"GooglePlayServices"})
    public void testCheckCallerIsValid() {
        Mockito.when(mMockPackageManager.getPackagesForUid(anyInt()))
                .thenReturn(mMockCallingPackages, mSystemOnlyMockPackages);
        assertFalse(
                "Returned true when checking system built for all packages",
                new ExternalAuthUtils().isCallerValid(ExternalAuthUtils.FLAG_SHOULD_BE_SYSTEM));
        assertTrue(
                "Returned false when checking system built for valid packages",
                new ExternalAuthUtils().isCallerValid(ExternalAuthUtils.FLAG_SHOULD_BE_SYSTEM));
    }
}
