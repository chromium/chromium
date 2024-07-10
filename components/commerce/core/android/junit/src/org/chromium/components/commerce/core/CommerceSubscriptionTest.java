// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.commerce.core;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for CommerceSubscription (particularly around equality and hashing). */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class CommerceSubscriptionTest {
    @Test
    public void testEqualityForDifferentObjects() {
        String clusterId = "1234";
        CommerceSubscription sub1 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);
        CommerceSubscription sub2 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);

        Assert.assertTrue(sub1.equals(sub2));
        Assert.assertTrue(sub2.equals(sub1));
    }

    @Test
    public void testHashEqualityForDifferentObjects() {
        String clusterId = "1234";
        CommerceSubscription sub1 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);
        CommerceSubscription sub2 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);

        Assert.assertEquals(sub1.hashCode(), sub2.hashCode());
    }

    @Test
    public void testNotEqual_ID() {
        CommerceSubscription sub1 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        "1234",
                        ManagementType.USER_MANAGED,
                        null);
        CommerceSubscription sub2 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        "5678",
                        ManagementType.USER_MANAGED,
                        null);

        Assert.assertFalse(sub1.equals(sub2));
        Assert.assertNotEquals(sub1.hashCode(), sub2.hashCode());
    }

    @Test
    public void testNotEqual_ManagementType() {
        String clusterId = "1234";
        CommerceSubscription sub1 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);
        CommerceSubscription sub2 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.CHROME_MANAGED,
                        null);

        Assert.assertFalse(sub1.equals(sub2));
        Assert.assertNotEquals(sub1.hashCode(), sub2.hashCode());
    }

    @Test
    public void testNotEqual_IdentifierType() {
        String clusterId = "1234";
        CommerceSubscription sub1 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);
        CommerceSubscription sub2 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.OFFER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);

        Assert.assertFalse(sub1.equals(sub2));
        Assert.assertNotEquals(sub1.hashCode(), sub2.hashCode());
    }

    @Test
    public void testNotEqual_SubscriptionType() {
        String clusterId = "1234";
        CommerceSubscription sub1 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);
        CommerceSubscription sub2 =
                new CommerceSubscription(
                        SubscriptionType.TYPE_UNSPECIFIED,
                        IdentifierType.OFFER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);

        Assert.assertFalse(sub1.equals(sub2));
        Assert.assertNotEquals(sub1.hashCode(), sub2.hashCode());
    }

    @Test
    public void testEquality_SeenOfferIgnored() {
        CommerceSubscription.UserSeenOffer seen =
                new CommerceSubscription.UserSeenOffer("5678", 100, "us", "en-US");

        String clusterId = "1234";
        CommerceSubscription sub1 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        null);
        CommerceSubscription sub2 =
                new CommerceSubscription(
                        SubscriptionType.PRICE_TRACK,
                        IdentifierType.PRODUCT_CLUSTER_ID,
                        clusterId,
                        ManagementType.USER_MANAGED,
                        seen);

        Assert.assertEquals(sub1.hashCode(), sub2.hashCode());
        Assert.assertTrue(sub1.equals(sub2));
    }
}
