// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.cronet_sample_apk;

import androidx.annotation.OptIn;

import org.chromium.net.ConnectionMigrationOptions;

import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;

/**
 * Adding an option here will make it show up in the list of available options. Each {@link Option}
 * has the following attributes:
 *
 * <ul>
 *   <li>A short name which appears in bold on the options list.
 *   <li>A description which provides a thorough explanation of what this option does.
 *   <li>An {@link Action} which is applied on CronetEngine's Builder each time the user hits "Reset
 *       Engine".
 *   <li>A default value, every option must have a default value.
 * </ul>
 *
 * <b>NOTE</b>: Each option must map to one {@link OptionsIdentifier OptionsIdentifier}. This is
 * necessary to provide custom implementation for options that does not configure the builders. See
 * {@link OptionsIdentifier#SLOW_DOWNLOAD} as an example.
 *
 * <p>To add a new option, do the following:
 *
 * <ol>
 *   <li>Add a new optionIdentifier {@link OptionsIdentifier}
 *   <li>Inject a new Option instance into your optionIdentifier enum value.
 *   <li>Implement the logic for the new option within a new {@link Action}.
 *   <li>If the {@link Action} interface is not enough to satisfy the use-case. Feel free to add
 *       custom logic, See {@link OptionsIdentifier#SLOW_DOWNLOAD} as an example.
 *   <li>Restart the APK and verify that your option is working as intended.
 * </ol>
 */
public class Options {
    @SuppressWarnings("ImmutableEnumChecker")
    public enum OptionsIdentifier {
        MIGRATE_SESSIONS_ON_NETWORK_CHANGE_V2(
                new BooleanOption(
                        "migrate_sessions_on_network_change_v2",
                        "Enable QUIC connection migration. This only occurs when a network has "
                                + "completed"
                                + " disconnected and no longer reachable. QUIC will try to migrate "
                                + "the current session(maybe idle? depending on another option) to "
                                + "another network.",
                        new Action<>() {
                            @Override
                            public void configureBuilder(ActionData data, Boolean value) {
                                data.getMigrationBuilder().enableDefaultNetworkMigration(value);
                            }
                        },
                        false)),
        MIGRATION_SESSION_EARLY_V2(
                new BooleanOption(
                        "migrate_sessions_early_v2",
                        "Enable QUIC early session migration. This will make quic send probing"
                            + " packets when the network is degrading, QUIC will migrate the"
                            + " sessions to a different network even before the original network"
                            + " has disconnected.",
                        new Action<Boolean>() {
                            @Override
                            @OptIn(markerClass = ConnectionMigrationOptions.Experimental.class)
                            public void configureBuilder(ActionData data, Boolean value) {
                                data.getMigrationBuilder().enablePathDegradationMigration(value);
                                data.getMigrationBuilder().allowNonDefaultNetworkUsage(value);
                            }
                        },
                        false)),
        SLOW_DOWNLOAD(
                new BooleanOption(
                        "Slow Download (10s)",
                        "Hang the onReadCompleted for 10s before proceeding. This should simulate"
                                + " slow connection.",
                        new Action<>() {},
                        false));

        private final Option<?> mOption;

        OptionsIdentifier(Option<?> option) {
            this.mOption = option;
        }

        public Option<?> getOption() {
            return mOption;
        }
    }

    private static final Map<OptionsIdentifier, Option> OPTIONS =
            Collections.unmodifiableMap(createOptionsMap());
    private static final List<Option> OPTION_LIST = List.copyOf(OPTIONS.values());

    private static Map<OptionsIdentifier, Option> createOptionsMap() {
        Map<OptionsIdentifier, Option> optionsMap = new LinkedHashMap<>();
        for (OptionsIdentifier optionIdentifier : OptionsIdentifier.values()) {
            optionsMap.put(optionIdentifier, optionIdentifier.getOption());
        }
        return optionsMap;
    }

    public abstract static class Option<T> {
        private final String mOptionName;
        private final String mOptionDescription;
        private final Action<T> mAction;
        private T mOptionValue;

        private Option(
                String optionName, String optionDescription, Action<T> action, T defaultValue) {
            this.mOptionName = optionName;
            this.mOptionDescription = optionDescription;
            this.mAction = action;
            this.mOptionValue = defaultValue;
        }

        public String getShortName() {
            return mOptionName;
        }

        public String getDescription() {
            return mOptionDescription;
        }

        void configure(ActionData data) {
            mAction.configureBuilder(data, getValue());
        }

        abstract Class<T> getInputType();

        T getValue() {
            return mOptionValue;
        }

        void setValue(T newValue) {
            mOptionValue = newValue;
        }
    }

    public static class BooleanOption extends Option<Boolean> {
        private BooleanOption(
                String optionName,
                String optionDescription,
                Action<Boolean> action,
                Boolean defaultValue) {
            super(optionName, optionDescription, action, defaultValue);
        }

        @Override
        Class<Boolean> getInputType() {
            return Boolean.class;
        }
    }

    private Options() {}

    public static List<Option> getOptions() {
        return OPTION_LIST;
    }

    public static boolean isBooleanOptionOn(OptionsIdentifier identifier) {
        if (!OPTIONS.containsKey(identifier)) {
            throw new IllegalArgumentException(
                    "The provided identifier does not map to any option");
        }
        if (!OPTIONS.get(identifier).getInputType().equals(Boolean.class)) {
            throw new IllegalStateException("The provided identifier maps to a non-boolean value");
        }
        return (boolean) OPTIONS.get(identifier).getValue();
    }
}
