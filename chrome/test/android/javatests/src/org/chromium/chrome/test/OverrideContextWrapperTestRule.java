// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.FeatureInfo;
import android.content.pm.PackageManager;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.PackageManagerWrapper;
import org.chromium.build.BuildConfig;

import java.util.ArrayList;
import java.util.List;

/**
 * JUnit test rule that takes care of setup and teardown for tests that need to override the
 * context.
 */
public class OverrideContextWrapperTestRule implements TestRule {
    private OverrideTestContext mContext;

    private class OverrideTestContext extends ContextWrapper {
        private Boolean mIsAutomotive;
        private Boolean mIsDesktop;

        public OverrideTestContext(Context baseContext) {
            super(baseContext);
        }

        public void setIsAutomotive(boolean isAutomotive) {
            this.mIsAutomotive = isAutomotive;
        }

        public void setIsDesktop(boolean isDesktop) {
            this.mIsDesktop = isDesktop;
            // (TODO: crbug.com/430983585) Clean up this flag once the desktop
            // build is fully functional.
            BuildConfig.IS_DESKTOP_ANDROID = isDesktop;
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public FeatureInfo[] getSystemAvailableFeatures() {
                    FeatureInfo[] features = super.getSystemAvailableFeatures();
                    if (mIsAutomotive == null && mIsDesktop == null) {
                        return features;
                    }
                    if (features == null
                            && !Boolean.TRUE.equals(mIsAutomotive)
                            && !Boolean.TRUE.equals(mIsDesktop)) {
                        return null;
                    }

                    List<FeatureInfo> overriddenFeatures = new ArrayList<>();
                    if (features != null) {
                        for (FeatureInfo feature : features) {
                            if (feature != null
                                    && ((mIsAutomotive != null
                                                    && PackageManager.FEATURE_AUTOMOTIVE.equals(
                                                            feature.name))
                                            || (mIsDesktop != null
                                                    && PackageManager.FEATURE_PC.equals( // nocheck
                                                            feature.name)))) {
                                continue;
                            }
                            overriddenFeatures.add(feature);
                        }
                    }
                    if (Boolean.TRUE.equals(mIsAutomotive)) {
                        FeatureInfo automotiveFeature = new FeatureInfo();
                        automotiveFeature.name = PackageManager.FEATURE_AUTOMOTIVE;
                        overriddenFeatures.add(automotiveFeature);
                    }
                    if (Boolean.TRUE.equals(mIsDesktop)) {
                        FeatureInfo pcFeature = new FeatureInfo();
                        pcFeature.name = PackageManager.FEATURE_PC; // nocheck
                        overriddenFeatures.add(pcFeature);
                    }
                    return overriddenFeatures.toArray(new FeatureInfo[0]);
                }

                // DeviceInfo still uses hasSystemFeature() below Android T and when the system
                // feature snapshot is unavailable.
                @Override
                public boolean hasSystemFeature(String name) {
                    if (mIsAutomotive != null && PackageManager.FEATURE_AUTOMOTIVE.equals(name)) {
                        return mIsAutomotive;
                    } else if (mIsDesktop != null && PackageManager.FEATURE_PC.equals(name)) {
                        return mIsDesktop;
                    }
                    return super.hasSystemFeature(name);
                }
            };
        }
    }

    public void setIsAutomotive(boolean isAutomotive) {
        mContext.setIsAutomotive(isAutomotive);
    }

    public void setIsDesktop(boolean isDesktop) {
        mContext.setIsDesktop(isDesktop);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Before
                Context contextToRestore = ContextUtils.getApplicationContext();
                boolean isDesktopToRestore = BuildConfig.IS_DESKTOP_ANDROID;
                mContext = new OverrideTestContext(contextToRestore);
                ContextUtils.initApplicationContextForTests(mContext);

                base.evaluate();

                // After DisableAnimationTestRule requires an initialized context to do proper
                // teardown.
                // This resets to the original context rather than nulling out.
                if (contextToRestore != null) {
                    ContextUtils.initApplicationContextForTests(contextToRestore);
                }
                // Also reset IS_DESKTOP_ANDROID to its original value.
                BuildConfig.IS_DESKTOP_ANDROID = isDesktopToRestore;
            }
        };
    }
}
