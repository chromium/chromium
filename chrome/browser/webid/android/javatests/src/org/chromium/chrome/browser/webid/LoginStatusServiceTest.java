// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.IBinder;

import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;

import java.util.List;

/** On-device instrumentation tests for {@link LoginStatusService}. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class LoginStatusServiceTest {
    @Test
    @MediumTest
    public void testQueryAndDispatchLoginStatusServiceOnRealDevice() {
        Context context = ContextUtils.getApplicationContext();
        PackageManager pm = context.getPackageManager();

        Intent actionIntent = new Intent("org.w3.FedCM.LOGIN_STATUS");
        List<ResolveInfo> services = pm.queryIntentServices(actionIntent, 0);
        assertFalse(
                "Real OS should discover LoginStatusService via intent filter", services.isEmpty());

        boolean foundAndVerified = false;
        for (ResolveInfo info : services) {
            if (info.serviceInfo != null
                    && LoginStatusService.class.getName().equals(info.serviceInfo.name)) {
                assertTrue(
                        "LoginStatusService must be exported in real OS manifest",
                        info.serviceInfo.exported);

                Intent bindIntent = new Intent("org.w3.FedCM.LOGIN_STATUS");
                bindIntent.setClassName(info.serviceInfo.packageName, info.serviceInfo.name);

                LoginStatusService service = new LoginStatusService();
                IBinder binder = service.onBind(bindIntent);
                assertNotNull(binder);
                assertTrue(binder instanceof LoginStatusService.LoginStatusBinder);
                LoginStatusService.LoginStatusBinder loginBinder =
                        (LoginStatusService.LoginStatusBinder) binder;
                assertTrue(loginBinder.setLoginStatus("logged-in", "https://idp.example"));
                foundAndVerified = true;
            }
        }
        assertTrue(
                "LoginStatusService must be found and verified on the physical device",
                foundAndVerified);
    }
}
