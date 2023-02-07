// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.content.Context;
import android.content.ContextWrapper;
import android.content.pm.PackageManager;

import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.PackageManagerWrapper;

/**
 * JUnit test rule that takes care of setup and teardown for automotive-specific tests.
 */
public class AutomotiveContextWrapperTestRule implements TestRule {
    private Context mContextToRestore;
    private AutomotiveTestContext mContext;

    private class AutomotiveTestContext extends ContextWrapper {
        private boolean mIsAutomotive;

        public AutomotiveTestContext(Context baseContext) {
            super(baseContext);
            mIsAutomotive = false;
        }

        public void setIsAutomotive(boolean isAutomotive) {
            this.mIsAutomotive = isAutomotive;
            BuildInfo.resetForTesting();
        }

        @Override
        public PackageManager getPackageManager() {
            return new PackageManagerWrapper(super.getPackageManager()) {
                @Override
                public boolean hasSystemFeature(String name) {
                    if (PackageManager.FEATURE_AUTOMOTIVE.equals(name)) {
                        return mIsAutomotive;
                    }
                    return super.hasSystemFeature(name);
                }
            };
        }
    }

    public void setIsAutomotive(boolean isAutomotive) {
        mContext.setIsAutomotive(isAutomotive);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return new Statement() {
            @Override
            public void evaluate() throws Throwable {
                // Before
                mContextToRestore = ContextUtils.getApplicationContext();
                mContext = new AutomotiveTestContext(mContextToRestore);
                ContextUtils.initApplicationContextForTests(mContext);

                base.evaluate();

                // After
                // DisableAnimationTestRule requires an initialized context to do proper teardown.
                // This resets to the original context rather than nulling out.
                if (mContextToRestore != null) {
                    ContextUtils.initApplicationContextForTests(mContextToRestore);
                }
                BuildInfo.resetForTesting();
            }
        };
    }
}