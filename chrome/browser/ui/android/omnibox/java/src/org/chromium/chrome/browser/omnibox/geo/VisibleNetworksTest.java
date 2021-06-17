// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.geo;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;

import android.util.Base64;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleCell.RadioType;
import org.chromium.chrome.browser.omnibox.geo.VisibleNetworks.VisibleWifi;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * Robolectric tests for {@link VisibleNetworks}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class VisibleNetworksTest {
    private static final String SSID1 = "ssid1";
    private static final String BSSID1 = "11:11:11:11:11:11";
    private static final Integer LEVEL1 = -1;
    private static final Long TIMESTAMP1 = 10L;
    private static final String SSID2 = "ssid2";
    private static final String BSSID2 = "11:11:11:11:11:12";
    private static final Integer LEVEL2 = -2;
    private static final Long TIMESTAMP2 = 20L;

    private final VisibleWifi mVisibleWifi1 = VisibleWifi.create(SSID1, BSSID1, LEVEL1, TIMESTAMP1);
    private final VisibleWifi mVisibleWifi2 = VisibleWifi.create(SSID2, BSSID2, LEVEL2, TIMESTAMP2);
    private final VisibleCell.Builder mVisibleCellCommunBuilder = VisibleCell.builder(RadioType.GSM)
                                                                          .setCellId(10)
                                                                          .setLocationAreaCode(11)
                                                                          .setMobileCountryCode(12)
                                                                          .setMobileNetworkCode(13);
    private final VisibleCell mVisibleCell1 = mVisibleCellCommunBuilder.setPhysicalCellId(14)
                                                      .setPrimaryScramblingCode(15)
                                                      .setTrackingAreaCode(16)
                                                      .setTimestamp(10L)
                                                      .build();
    private final VisibleCell mVisibleCell1DifferentTimestamp =
            mVisibleCellCommunBuilder.setTimestamp(20L).build();
    private final VisibleCell mVisibleCell2 = VisibleCell.builder(RadioType.GSM)
                                                      .setCellId(30)
                                                      .setLocationAreaCode(31)
                                                      .setMobileCountryCode(32)
                                                      .setMobileNetworkCode(33)
                                                      .setTimestamp(30L)
                                                      .build();
    private final VisibleCell mEmptyCell = VisibleCell.builder(RadioType.UNKNOWN).build();
    private final VisibleWifi mEmptyWifi = VisibleWifi.create(null, null, null, null);

    private final Set<VisibleCell> mAllVisibleCells =
            new HashSet<>(Arrays.asList(mVisibleCell1, mVisibleCell2));
    private final Set<VisibleCell> mAllVisibleCells2 = new HashSet<>(
            Arrays.asList(mVisibleCell1, mVisibleCell2, mVisibleCell1DifferentTimestamp));
    private final Set<VisibleWifi> mAllVisibleWifis =
            new HashSet<>(Arrays.asList(mVisibleWifi1, mVisibleWifi2));

    private final VisibleNetworks mVisibleNetworks1 = VisibleNetworks.create(
            mVisibleWifi1, mVisibleCell1, mAllVisibleWifis, mAllVisibleCells);

    private final VisibleNetworks mVisibleNetworks2 = VisibleNetworks.create(
            mVisibleWifi2, mVisibleCell2, mAllVisibleWifis, mAllVisibleCells2);

    private static final String VISIBLE_CELL1_PROTO_ENCODED =
            "CAEQDLoBFhIQCAEQChgLIAwoDTAPOA5AEBgBIAo=";
    private static final String VISIBLE_WIFI1_PROTO_ENCODED =
            "CAEQDLoBJAoeChExMToxMToxMToxMToxMToxMRD___________8BGAEgCg==";
    private static final String EMPTY_CELL_PROTO_ENCODED = "CAEQDLoBBhICCAAYAQ==";
    private static final String EMPTY_WIFI_PROTO_ENCODED = "CAEQDLoBBAoAGAE=";

    @Test
    public void testVisibleWifiCreate() {
        VisibleWifi visibleWifi = VisibleWifi.create(SSID1, BSSID1, LEVEL1, TIMESTAMP1);
        assertEquals(SSID1, visibleWifi.ssid());
        assertEquals(BSSID1, visibleWifi.bssid());
        assertEquals(LEVEL1, visibleWifi.level());
        assertEquals(TIMESTAMP1, visibleWifi.timestampMs());
    }

    @Test
    public void testVisibleWifiEquals() {
        VisibleWifi copyOfVisibleWifi1 = VisibleWifi.create(mVisibleWifi1.ssid(),
                mVisibleWifi1.bssid(), mVisibleWifi1.level(), mVisibleWifi1.timestampMs());

        assertEquals(mVisibleWifi1, copyOfVisibleWifi1);
        assertNotEquals(mVisibleWifi1, mVisibleWifi2);
    }

    @Test
    public void testVisibleWifiEqualsDifferentLevelAndTimestamp() {
        VisibleWifi visibleWifi3 = VisibleWifi.create(SSID2, BSSID2, LEVEL1, TIMESTAMP1);

        // visibleWifi3 has the same ssid and bssid as mVisibleWifi2 but different level and
        // timestamp. The level and timestamp are excluded from the VisibleWifi equality checks.
        assertEquals(visibleWifi3, mVisibleWifi2);
    }

    @Test
    public void testVisibleWifiHash() {
        VisibleWifi copyOfVisibleWifi1 = VisibleWifi.create(mVisibleWifi1.ssid(),
                mVisibleWifi1.bssid(), mVisibleWifi1.level(), mVisibleWifi1.timestampMs());

        assertEquals(mVisibleWifi1.hashCode(), copyOfVisibleWifi1.hashCode());
        assertNotEquals(mVisibleWifi1.hashCode(), mVisibleWifi2.hashCode());
    }

    @Test
    public void testVisibleWifiHashDifferentLevelAndTimestamp() {
        VisibleWifi visibleWifi3 = VisibleWifi.create(SSID2, BSSID2, LEVEL1, TIMESTAMP1);
        // visibleWifi3 has the same ssid and bssid as mVisibleWifi2 but different level and
        // timestamp. The level and timestamp are excluded from the VisibleWifi hash function.
        assertEquals(mVisibleWifi2.hashCode(), visibleWifi3.hashCode());
    }

    @Test
    public void testVisibleWifiToProto() {
        boolean connected = true;
        PartnerLocationDescriptor.VisibleNetwork visibleNetwork = mVisibleWifi1.toProto(connected);
        PartnerLocationDescriptor.VisibleNetwork.WiFi wifi = visibleNetwork.getWifi();

        assertEquals(mVisibleWifi1.bssid(), wifi.getBssid());
        assertEquals(mVisibleWifi1.level().intValue(), wifi.getLevelDbm());
        assertEquals(mVisibleWifi1.timestampMs().longValue(), visibleNetwork.getTimestampMs());
        assertEquals(connected, visibleNetwork.getConnected());

        assertEquals(VISIBLE_WIFI1_PROTO_ENCODED, encodeVisibleNetwork(visibleNetwork));
    }

    @Test
    public void testVisibleWifiToProtoEmptyWifi() {
        boolean connected = true;
        PartnerLocationDescriptor.VisibleNetwork visibleNetwork = mEmptyWifi.toProto(connected);
        PartnerLocationDescriptor.VisibleNetwork.WiFi wifi = visibleNetwork.getWifi();

        assertFalse(wifi.hasBssid());
        assertFalse(wifi.hasLevelDbm());
        assertFalse(visibleNetwork.hasTimestampMs());
        assertEquals(connected, visibleNetwork.getConnected());

        assertEquals(EMPTY_WIFI_PROTO_ENCODED, encodeVisibleNetwork(visibleNetwork));
    }

    @Test
    public void testVisibleCellBuilder() {
        for (@RadioType int radioType = RadioType.UNKNOWN; radioType < RadioType.NUM_ENTRIES;
                radioType++) {
            VisibleCell visibleCell = VisibleCell.builder(radioType).build();
            assertEquals(radioType, visibleCell.radioType());
        }
    }

    @Test
    public void testVisibleCellEquals() {
        VisibleCell copyOfVisibleCell1 =
                VisibleCell.builder(mVisibleCell1.radioType())
                        .setCellId(mVisibleCell1.cellId())
                        .setLocationAreaCode(mVisibleCell1.locationAreaCode())
                        .setMobileCountryCode(mVisibleCell1.mobileCountryCode())
                        .setMobileNetworkCode(mVisibleCell1.mobileNetworkCode())
                        .setPhysicalCellId(mVisibleCell1.physicalCellId())
                        .setPrimaryScramblingCode(mVisibleCell1.primaryScramblingCode())
                        .setTrackingAreaCode(mVisibleCell1.trackingAreaCode())
                        .setTimestamp(mVisibleCell1.timestampMs())
                        .build();
        assertNotEquals(mVisibleCell2, mVisibleCell1);
        assertEquals(mVisibleCell1, copyOfVisibleCell1);
    }

    @Test
    public void testVisibleCellEqualsDifferentTimestamp() {
        // The timestamp is not included in the VisibleCell equality checks.
        assertEquals(mVisibleCell1, mVisibleCell1DifferentTimestamp);
    }

    @Test
    public void testVisibleCellHash() {
        VisibleCell copyOfVisibleCell1 =
                VisibleCell.builder(mVisibleCell1.radioType())
                        .setCellId(mVisibleCell1.cellId())
                        .setLocationAreaCode(mVisibleCell1.locationAreaCode())
                        .setMobileCountryCode(mVisibleCell1.mobileCountryCode())
                        .setMobileNetworkCode(mVisibleCell1.mobileNetworkCode())
                        .setPhysicalCellId(mVisibleCell1.physicalCellId())
                        .setPrimaryScramblingCode(mVisibleCell1.primaryScramblingCode())
                        .setTrackingAreaCode(mVisibleCell1.trackingAreaCode())
                        .setTimestamp(mVisibleCell1.timestampMs())
                        .build();

        assertEquals(mVisibleCell1.hashCode(), copyOfVisibleCell1.hashCode());
        assertNotEquals(mVisibleCell2.hashCode(), mVisibleCell1.hashCode());
    }

    @Test
    public void testVisibleCellHashDifferentTimestamp() {
        // The timestamp is not included in the VisibleCell hash function.
        assertEquals(mVisibleCell1.hashCode(), mVisibleCell1DifferentTimestamp.hashCode());
    }

    @Test
    public void testVisibleCellToProto() {
        boolean connected = true;
        PartnerLocationDescriptor.VisibleNetwork visibleNetwork = mVisibleCell1.toProto(connected);
        PartnerLocationDescriptor.VisibleNetwork.Cell cell = visibleNetwork.getCell();

        assertEquals(mVisibleCell1.cellId().intValue(), cell.getCellId());
        assertEquals(mVisibleCell1.locationAreaCode().intValue(), cell.getLocationAreaCode());
        assertEquals(mVisibleCell1.mobileCountryCode().intValue(), cell.getMobileCountryCode());
        assertEquals(mVisibleCell1.mobileNetworkCode().intValue(), cell.getMobileNetworkCode());
        assertEquals(
                mVisibleCell1.primaryScramblingCode().intValue(), cell.getPrimaryScramblingCode());
        assertEquals(mVisibleCell1.physicalCellId().intValue(), cell.getPhysicalCellId());
        assertEquals(mVisibleCell1.trackingAreaCode().intValue(), cell.getTrackingAreaCode());
        assertEquals(mVisibleCell1.timestampMs().longValue(), visibleNetwork.getTimestampMs());
        assertEquals(connected, visibleNetwork.getConnected());
        assertEquals(PartnerLocationDescriptor.VisibleNetwork.Cell.Type.GSM, cell.getType());

        assertEquals(VISIBLE_CELL1_PROTO_ENCODED, encodeVisibleNetwork(visibleNetwork));
    }

    @Test
    public void testVisibleCellToProtoEmptyCell() {
        boolean connected = true;
        PartnerLocationDescriptor.VisibleNetwork visibleNetwork = mEmptyCell.toProto(connected);
        PartnerLocationDescriptor.VisibleNetwork.Cell cell = visibleNetwork.getCell();

        assertEquals(PartnerLocationDescriptor.VisibleNetwork.Cell.Type.UNKNOWN, cell.getType());
        assertFalse(cell.hasCellId());
        assertFalse(cell.hasLocationAreaCode());
        assertFalse(cell.hasMobileCountryCode());
        assertFalse(cell.hasMobileNetworkCode());
        assertFalse(cell.hasPrimaryScramblingCode());
        assertFalse(cell.hasPhysicalCellId());
        assertFalse(cell.hasTrackingAreaCode());
        assertFalse(visibleNetwork.hasTimestampMs());
        assertEquals(connected, visibleNetwork.getConnected());

        assertEquals(EMPTY_CELL_PROTO_ENCODED, encodeVisibleNetwork(visibleNetwork));
    }

    @Test
    public void testVisibleNetworksCreate() {
        Set<VisibleCell> expectedVisibleCells =
                new HashSet<VisibleCell>(Arrays.asList(mVisibleCell1, mVisibleCell2));
        Set<VisibleWifi> expectedVisibleWifis =
                new HashSet<VisibleWifi>(Arrays.asList(mVisibleWifi1, mVisibleWifi2));
        VisibleNetworks visibleNetworks = VisibleNetworks.create(
                mVisibleWifi1, mVisibleCell1, expectedVisibleWifis, expectedVisibleCells);
        assertEquals(mVisibleWifi1, visibleNetworks.connectedWifi());
        assertEquals(mVisibleCell1, visibleNetworks.connectedCell());
        assertEquals(expectedVisibleWifis, visibleNetworks.allVisibleWifis());
        assertEquals(expectedVisibleCells, visibleNetworks.allVisibleCells());
    }

    @Test
    public void testVisibleNetworksEquals() {
        VisibleNetworks copyOfVisibleNetworks1 = VisibleNetworks.create(
                mVisibleNetworks1.connectedWifi(), mVisibleNetworks1.connectedCell(),
                mVisibleNetworks1.allVisibleWifis(), mVisibleNetworks1.allVisibleCells());

        assertEquals(mVisibleNetworks1, copyOfVisibleNetworks1);
        assertNotEquals(mVisibleNetworks1, mVisibleNetworks2);
    }

    @Test
    public void testVisibleNetworksHash() {
        VisibleNetworks copyOfVisibleNetworks1 = VisibleNetworks.create(
                mVisibleNetworks1.connectedWifi(), mVisibleNetworks1.connectedCell(),
                mVisibleNetworks1.allVisibleWifis(), mVisibleNetworks1.allVisibleCells());

        assertEquals(mVisibleNetworks1.hashCode(), copyOfVisibleNetworks1.hashCode());
        assertNotEquals(mVisibleNetworks1.hashCode(), mVisibleNetworks2.hashCode());
    }

    @Test
    public void testVisibleNetworksIsEmpty() {
        VisibleNetworks visibleNetworks = VisibleNetworks.create(null, null, null, null);
        assertTrue(visibleNetworks.isEmpty());
        assertFalse(mVisibleNetworks1.isEmpty());
    }

    private static String encodeVisibleNetwork(
            PartnerLocationDescriptor.VisibleNetwork visibleNetwork) {
        PartnerLocationDescriptor.LocationDescriptor locationDescriptor =
                PartnerLocationDescriptor.LocationDescriptor.newBuilder()
                        .setRole(PartnerLocationDescriptor.LocationRole.CURRENT_LOCATION)
                        .setProducer(PartnerLocationDescriptor.LocationProducer.DEVICE_LOCATION)
                        .addVisibleNetwork(visibleNetwork)
                        .build();

        return Base64.encodeToString(
                locationDescriptor.toByteArray(), Base64.NO_WRAP | Base64.URL_SAFE);
    }
}
