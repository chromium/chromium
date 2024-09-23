// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.edge_to_edge;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;

/** Conditions that provides edge to edge related classes when features are enabled. */
class EdgeToEdgeConditions {

    /**
     * Ensure the edge to edge controller is available for the test, representing the test is
     * running on a device that supports edge to edge.
     */
    static class EdgeToEdgeControllerCondition extends ConditionWithResult<EdgeToEdgeController> {

        private final Supplier<ChromeTabbedActivity> mActivitySupplier;

        /**
         * @param activitySupplier Supplier to the hosting CTA.
         */
        public EdgeToEdgeControllerCondition(Supplier<ChromeTabbedActivity> activitySupplier) {
            super(/* isRunOnUiThread= */ true);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
        }

        @Override
        protected ConditionStatusWithResult<EdgeToEdgeController> resolveWithSuppliers()
                throws Exception {
            EdgeToEdgeController e2eController =
                    mActivitySupplier.get().getEdgeToEdgeSupplier().get();
            if (e2eController != null) {
                return fulfilled().withResult(e2eController);
            } else {
                return awaiting("Activity has no e2eController").withoutResult();
            }
        }

        @Override
        public String buildDescription() {
            return "EdgeToEdgeController is available.";
        }
    }

    /** Condition that checks the bottom browser control stacker. */
    static class BottomControlsStackerCondition extends ConditionWithResult<BottomControlsStacker> {
        private final Supplier<ChromeTabbedActivity> mActivitySupplier;

        /**
         * @param activitySupplier Supplier to the hosting CTA.
         */
        public BottomControlsStackerCondition(Supplier<ChromeTabbedActivity> activitySupplier) {
            super(/* isRunOnUiThread= */ true);
            mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
        }

        @Override
        protected ConditionStatusWithResult<BottomControlsStacker> resolveWithSuppliers()
                throws Exception {
            var rootUiCoordinator = mActivitySupplier.get().getRootUiCoordinatorForTesting();
            if (rootUiCoordinator != null) {
                return fulfilled()
                        .withResult(rootUiCoordinator.getBottomControlsStackerForTesting());
            } else {
                return awaiting("Activity has no root UI coordinator yet.").withoutResult();
            }
        }

        @Override
        public String buildDescription() {
            return "BottomControlsStacker is available.";
        }
    }
}
