// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.settings.search;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.os.Bundle;
import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.text.Normalizer;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Queue;
import java.util.Set;
import java.util.regex.Pattern;

/** Data from Preferences used for settings search. This is a collection of data to be indexed. */
@NullMarked
public class SettingsIndexData {
    private static final String TAG = "SettingsIndexData";

    /* Scores given to the entries having matches with a query */
    public static final int EXACT_TITLE_MATCH = 10;
    public static final int PARTIAL_TITLE_MATCH = 5;
    public static final int PARTIAL_SUMMARY_MATCH = 3;
    public static final int PARTIAL_KEYWORD_MATCH = 2;

    // Regular expression to remove diacritics.
    private static final Pattern STRIP_ACCENTS_PATTERN = Pattern.compile("\\p{M}");

    private final Map<String, Entry> mEntries = new LinkedHashMap<>();
    private final Set<String> mRemovedKeys = new HashSet<>();
    // Map from a child fragment's class name to the list of preference keys that can link to it.
    private final Map<String, List<String>> mChildFragmentToParentKeys = new HashMap<>();

    private static @Nullable SettingsIndexData sInstance;
    private static boolean sNeedsIndexing = true;

    // LINT.IfChange(MainSettingsClassName)
    private static final String sMainSettingsClassName =
            "org.chromium.chrome.browser.settings.MainSettings";

    // LINT.ThenChange(//chrome/android/chrome_java_sources.gni:MainSettingsBuildRule)

    // Whether the search results needs refreshing when coming back from result-browsing state
    // to search state. It is possible for the index to be modified while browsing search results.
    // In such case the search result display needs refreshing accordingly; otherwise it could
    // show the results that are already hidden, or vice versa.
    private boolean mShouldRefreshResult;

    @EnsuresNonNull("sInstance")
    public static SettingsIndexData createInstance() {
        assert sInstance == null;
        sInstance = new SettingsIndexData();
        return sInstance;
    }

    public static @Nullable SettingsIndexData getInstance() {
        return sInstance;
    }

    public static void reset() {
        sInstance = null;
        sNeedsIndexing = true;
    }

    /**
     * Normalizes a string for search by converting it to lowercase and stripping diacritics.
     *
     * @param input The string to normalize.
     * @return The normalized string, or null if the input was null.
     */
    private static @Nullable String normalizeString(@Nullable String input) {
        if (input == null) return null;

        // 1. Decompose characters into base letters and combining accent marks.
        String decomposed = Normalizer.normalize(input, Normalizer.Form.NFD);
        // 2. Remove the combining accent marks using a regular expression.
        String stripped = STRIP_ACCENTS_PATTERN.matcher(decomposed).replaceAll("");
        // 3. Convert to lowercase for case-insensitive matching.
        return stripped.toLowerCase(Locale.getDefault());
    }

    /**
     * An immutable data model representing a single searchable preference.
     *
     * <p>This is a value object that holds all the necessary information for both indexing and
     * displaying a search result. Its public fields are final to guarantee immutability, ensuring
     * that its state cannot be changed after creation.
     */
    public static class Entry implements Parcelable {
        /** The entry's globally unique id. */
        public final String id;

        /** The original key defined for Preference/Fragment. */
        public final String key;

        /** Title of Preference/Fragment. */
        public final @Nullable String title;

        /** Summary/description of Preference/Fragment. */
        public final @Nullable String summary;

        /** Package path name if the entry is Fragment. Otherwise {@code null}. */
        public final @Nullable String fragment;

        /** List of keywords relevant to the preference entry. */
        public final @Nullable String @Nullable [] keywords;

        /** Key of the preference/fragment to highlight if it is not the same as {@code key}. */
        public final @Nullable String highlightKey;

        /** Zero-based index of the view to highlight if preference has multiple child views. */
        public final int subViewPos;

        /**
         * Top-level setting entry where this entry belongs, such as Privacy and security, Payment,
         * Languages.
         */
        public final @Nullable String header;

        /** Package path name of the immediate parent Fragment of this entry. */
        public final String parentFragment;

        /** Extra arguments needed to launch a pref. */
        public final Bundle extras;

        /** Boolean flag indicating the entry's searchability. */
        public final boolean isSearchable;

        private final @Nullable String mTitleNormalized;
        private final @Nullable String mSummaryNormalized;

        private Entry(
                String id,
                String key,
                @Nullable String title,
                @Nullable String header,
                @Nullable String summary,
                @Nullable String fragment,
                @Nullable String @Nullable [] keywords,
                @Nullable String highlightKey,
                int subViewPos,
                Bundle extras,
                String parentFragment,
                boolean isSearchable,
                @Nullable String titleNormalized,
                @Nullable String summaryNormalized) {
            this.id = id;
            this.key = key;
            this.title = title;
            this.header = header;
            this.summary = summary;
            this.fragment = fragment;
            this.keywords = keywords;
            this.highlightKey = highlightKey;
            this.subViewPos = subViewPos;
            this.extras = extras;
            this.parentFragment = parentFragment;
            this.isSearchable = isSearchable;
            mTitleNormalized = titleNormalized;
            mSummaryNormalized = summaryNormalized;
        }

        // Parcel Constructor
        protected Entry(Parcel in) {
            id = assumeNonNull(in.readString());
            key = in.readString();
            title = in.readString();
            summary = in.readString();
            fragment = in.readString();
            keywords = in.createStringArray();
            highlightKey = in.readString();
            subViewPos = in.readInt();
            header = in.readString();
            parentFragment = in.readString();
            // Bundles require a ClassLoader to unparcel custom classes inside them
            Bundle inExtras = in.readBundle(getClass().getClassLoader());
            extras = inExtras != null ? inExtras : new Bundle();
            isSearchable = in.readByte() != 0;
            mTitleNormalized = in.readString();
            mSummaryNormalized = in.readString();
        }

        /**
         * Returns the entry in JSON object format. This is primarily used to store the entry to
         * disk.
         */
        public @Nullable JSONObject toJsonObject() {
            try {
                JSONObject jsonObject = new JSONObject();
                jsonObject.put("id", id);
                jsonObject.put("key", key);
                jsonObject.put("title", title);
                jsonObject.put("summary", summary);
                jsonObject.put("fragment", fragment);
                jsonObject.put("highlightKey", highlightKey);
                jsonObject.put("subViewPos", subViewPos);
                jsonObject.put("extras", extrasToString(extras));
                jsonObject.put("parentFragment", parentFragment);
                return jsonObject;
            } catch (JSONException e) {
                Log.e(TAG, "Error converting Entry to JSON object.");
            }
            return null;
        }

        /** Build {@link Entry} from a given JSON object. */
        public static @Nullable Entry fromJson(JSONObject obj) {
            try {
                String id = obj.getString("id");
                String key = obj.getString("key");
                String title = obj.optString("title", null);
                String summary = obj.optString("summary", null);
                String fragment = obj.optString("fragment", null);
                String highlightKey = obj.optString("highlightKey", null);
                int subViewPos = obj.optInt("subViewPos", -1);
                Bundle extras = stringToExtras(obj.optString("extras", ""));
                String parentFragment = obj.optString("parentFragment", null);
                var builder =
                        new SettingsIndexData.Entry.Builder(id, key, title, parentFragment)
                                .setSummary(summary)
                                .setFragment(fragment);
                if (subViewPos >= 0) builder.setSubViewPos(subViewPos);
                if (highlightKey != null) builder.setHighlightKey(highlightKey);
                if (extras != null) builder.setArguments(extras);
                return builder.build();
            } catch (JSONException e) {
                Log.e(TAG, "Error building Entry from JSON object.");
            }
            return null;
        }

        private static @Nullable String extrasToString(@Nullable Bundle extras) {
            if (extras == null) return null;
            JSONObject json = new JSONObject();

            for (String key : extras.keySet()) {
                try {
                    json.put(key, extras.get(key));
                } catch (JSONException e) {
                    Log.e(TAG, "Error converting extras to JSON.");
                }
            }
            return json.toString();
        }

        private static @Nullable Bundle stringToExtras(String jsonString)
                throws IllegalArgumentException {
            JSONObject json;
            try {
                json = new JSONObject(jsonString);
            } catch (JSONException e) {
                Log.e(TAG, "Error restoring Bundle from JSON string.");
                return null;
            }
            Iterator<String> keys = json.keys();
            Bundle bundle = new Bundle();
            while (keys.hasNext()) {
                String key = keys.next();
                try {
                    Object value = json.get(key);
                    if (value instanceof Integer) {
                        bundle.putInt(key, (Integer) value);
                    } else if (value instanceof Long) {
                        bundle.putLong(key, (Long) value);
                    } else if (value instanceof Double) {
                        bundle.putDouble(key, (Double) value);
                    } else if (value instanceof Boolean) {
                        bundle.putBoolean(key, (Boolean) value);
                    } else if (value instanceof String) {
                        bundle.putString(key, (String) value);
                    } else {
                        // Complex types are not expected in the Bundle object used for extras.
                        // Report the exception if it actually occurs.
                        throw new IllegalArgumentException(
                                "Unsupported complex type in extras: " + key);
                    }
                } catch (JSONException e) {
                    // Skip restoring the element that causes an exception.
                    Log.e(TAG, "Error restoring Bundle from JSON string.");
                }
            }
            return bundle;
        }

        @Override
        public void writeToParcel(Parcel dest, int flags) {
            dest.writeString(id);
            dest.writeString(key);
            dest.writeString(title);
            dest.writeString(summary);
            dest.writeString(fragment);
            dest.writeStringArray(keywords);
            dest.writeString(highlightKey);
            dest.writeInt(subViewPos);
            dest.writeString(header);
            dest.writeString(parentFragment);
            dest.writeBundle(extras);
            dest.writeByte((byte) (isSearchable ? 1 : 0));
            dest.writeString(mTitleNormalized);
            dest.writeString(mSummaryNormalized);
        }

        @Override
        public int describeContents() {
            return 0;
        }

        public static final Creator<Entry> CREATOR =
                new Creator<Entry>() {
                    @Override
                    public Entry createFromParcel(Parcel in) {
                        return new Entry(in);
                    }

                    @Override
                    public Entry[] newArray(int size) {
                        return new Entry[size];
                    }
                };

        /**
         * A builder for creating immutable {@link Entry} objects. For future modifications, please
         * be aware of the normalized fields.
         */
        public static class Builder {
            private final String mId;
            private final String mKey;
            private @Nullable String mTitle;
            private @Nullable String mHeader;
            private @Nullable String mSummary;
            private @Nullable String mFragment;
            private @Nullable String @Nullable [] mKeywords;
            private @Nullable String mHighlightKey;
            private int mSubViewPos;
            private Bundle mExtras;
            private final String mParentFragment;
            private boolean mIsSearchable = true;

            /**
             * Constructs a builder with the minimum required fields for creating a new {@link
             * Entry}.
             *
             * @param id The unique id of the preference.
             * @param key The key of the preference.
             * @param title The title of the preference.
             * @param parentFragment The class name of the fragment containing this preference.
             */
            public Builder(String id, String key, @Nullable String title, String parentFragment) {
                mId = id;
                mKey = key;
                mTitle = title;
                mParentFragment = parentFragment;
                mExtras = new Bundle();
            }

            /**
             * Constructs a builder by copying the state from an existing {@link Entry}.
             *
             * @param original The original {@link Entry} to copy.
             */
            public Builder(Entry original) {
                mId = original.id;
                mKey = original.key;
                mTitle = original.title;
                mHeader = original.header;
                mSummary = original.summary;
                mFragment = original.fragment;
                mKeywords = original.keywords;
                mHighlightKey = original.highlightKey;
                mSubViewPos = original.subViewPos;
                mExtras = original.extras;
                mParentFragment = original.parentFragment;
                mIsSearchable = original.isSearchable;
            }

            public Builder setTitle(@Nullable String title) {
                mTitle = title;
                return this;
            }

            public Builder setHeader(@Nullable String header) {
                mHeader = header;
                return this;
            }

            public Builder setSummary(@Nullable String summary) {
                // Removes tags before storing.
                mSummary = summary != null ? summary.replaceAll("<[^>]*>", "") : summary;
                return this;
            }

            public Builder setFragment(@Nullable String fragment) {
                mFragment = fragment;
                return this;
            }

            public Builder setKeywords(@Nullable String keywords) {
                mKeywords =
                        keywords != null
                                ? assumeNonNull(normalizeString(keywords)).split("\\s*,\\s*")
                                : null;
                return this;
            }

            public Builder setHighlightKey(String key) {
                mHighlightKey = key;
                return this;
            }

            public Builder setSubViewPos(int viewPos) {
                mSubViewPos = viewPos;
                return this;
            }

            public Builder setArguments(Bundle extras) {
                mExtras = extras;
                return this;
            }

            public Builder setIsSearchable(boolean isSearchable) {
                mIsSearchable = isSearchable;
                return this;
            }

            /**
             * Creates an {@link Entry} object.
             *
             * @return A new, immutable {@link Entry} instance.
             */
            public Entry build() {
                String titleNormalized = normalizeString(mTitle);
                String summaryNormalized = normalizeString(mSummary);

                return new Entry(
                        mId,
                        mKey,
                        mTitle,
                        mHeader,
                        mSummary,
                        mFragment,
                        mKeywords,
                        mHighlightKey,
                        mSubViewPos,
                        mExtras,
                        mParentFragment,
                        mIsSearchable,
                        titleNormalized,
                        summaryNormalized);
            }
        }
    }

    /**
     * Adds a new searchable preference entry to the index.
     *
     * @param id The unique ID of the preference. This is used as the primary identifier in the
     *     internal map.
     * @param entry The {@link Entry} object containing all the data for this preference.
     * @throws IllegalStateException If a preference with the same key already exists in the index.
     */
    public void addEntry(String id, Entry entry) {
        assert PreferenceParser.isId(id) : "Use getUniqueId(key) to pass a unique id.";
        if (mEntries.containsKey(id)) {
            throw new IllegalStateException("Duplicate ID found: " + id);
        }
        mEntries.put(id, entry);

        if (!TextUtils.isEmpty(entry.fragment)) {
            addChildParentLink(entry.fragment, id);
        }
    }

    /**
     * Adds a new searchable preference entry to the index.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param titleId String resource ID of the title.
     */
    public void addEntryForKey(String prefFragment, String key, int titleId) {
        addEntryForKey(prefFragment, key, titleId, /* summaryId= */ 0);
    }

    /**
     * Adds a new searchable preference entry to the index.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param titleId String resource ID of the title.
     * @param summaryId String resource ID of the summary.
     */
    public void addEntryForKey(String prefFragment, String key, int titleId, int summaryId) {
        Context context = ContextUtils.getApplicationContext();
        addEntryForKey(
                prefFragment,
                key,
                context.getString(titleId),
                summaryId != 0 ? context.getString(summaryId) : null);
    }

    /**
     * Adds a new searchable preference entry to the index.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param titleId String resource ID of the title.
     * @param summaryId String resource ID of the summary.
     * @param extras Extra bundle to pass to the Fragment.
     */
    public void addEntryForKey(
            String prefFragment, String key, int titleId, int summaryId, Bundle extras) {
        Context context = ContextUtils.getApplicationContext();
        addEntryForKey(
                prefFragment,
                key,
                context.getString(titleId),
                summaryId != 0 ? context.getString(summaryId) : null,
                extras);
    }

    /**
     * Adds a new searchable preference entry to the index.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param title Title text.
     * @param summary Summary text.
     */
    public void addEntryForKey(
            String prefFragment, String key, String title, @Nullable String summary) {
        addEntryForKey(prefFragment, key, title, summary, /* extras= */ null);
    }

    /**
     * Adds a new searchable preference entry that launches a specific fragment.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param titleId String resource ID of the title.
     * @param summaryId String resource ID of the summary.
     * @param targetFragment Full class name of the child Fragment this entry opens.
     */
    public void addEntryForKey(
            String prefFragment, String key, int titleId, int summaryId, String targetFragment) {
        Context context = ContextUtils.getApplicationContext();
        addEntryForKey(
                prefFragment,
                key,
                context.getString(titleId),
                summaryId != 0 ? context.getString(summaryId) : null,
                /* extras= */ null,
                targetFragment);
    }

    /**
     * Adds a new searchable preference entry to the index.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param title Title text.
     * @param summary Summary text.
     * @param extras Extra bundle to pass to the Fragment.
     * @param targetFragment Full class name of the child Fragment this entry opens.
     */
    public void addEntryForKey(
            String prefFragment,
            String key,
            String title,
            @Nullable String summary,
            @Nullable Bundle extras) {
        addEntryForKey(
                prefFragment,
                key,
                title,
                summary,
                /* extras= */ extras,
                /* targetFragment= */ null);
    }

    /**
     * Adds a new searchable preference entry to the index.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param title Title text.
     * @param summary Summary text.
     * @param extras Extra bundle to pass to the Fragment.
     * @param targetFragment Full class name of the child Fragment this entry opens.
     */
    public void addEntryForKey(
            String prefFragment,
            String key,
            String title,
            @Nullable String summary,
            @Nullable Bundle extras,
            @Nullable String targetFragment) {
        String id = PreferenceParser.createUniqueId(prefFragment, key);
        var builder = new Entry.Builder(id, key, title, prefFragment);
        if (summary != null) builder.setSummary(summary);
        if (extras != null) builder.setArguments(extras);
        if (targetFragment != null) builder.setFragment(targetFragment);
        addEntry(id, builder.build());
    }

    public @Nullable Entry getEntry(String id) {
        return mEntries.get(id);
    }

    /**
     * Gets a preference entry of a given key from the index, if exists.
     *
     * @param prefFragment Full class name of the Fragment where the key belongs.
     * @param key Key name of the preference entry.
     * @return entry The entry if it exists, null otherwise.
     */
    public @Nullable Entry getEntryForKey(String prefFragment, String key) {
        return getEntry(PreferenceParser.createUniqueId(prefFragment, key));
    }

    /**
     * Replaces an existing entry with a new one.
     *
     * @param id The ID of the {@link Entry} to replace.
     * @param updatedEntry The new {@link Entry} to place in place of the existing one.
     */
    public void updateEntry(String id, Entry updatedEntry) {
        assert PreferenceParser.isId(id) : "Use getUniqueId(key) to pass a unique id.";
        mEntries.put(id, updatedEntry);

        if (!TextUtils.isEmpty(updatedEntry.fragment)) {
            addChildParentLink(updatedEntry.fragment, id);
        }
    }

    /**
     * Replaces an existing entry with a new one.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param titleId String resource ID of the title.
     * @throws IllegalStateException If a preference with the same key does not exist in the index.
     */
    public void updateEntryForKey(String prefFragment, String key, int titleId) {
        updateEntryForKey(prefFragment, key, titleId, null);
    }

    /**
     * Replaces an existing entry with a new one.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param titleId String resource ID of the title.
     * @param targetFragment Full class name of the child Fragment this entry opens.
     * @throws IllegalStateException If a preference with the same key does not exist in the index.
     */
    public void updateEntryForKey(
            String prefFragment, String key, int titleId, @Nullable String targetFragment) {
        String id = PreferenceParser.createUniqueId(prefFragment, key);
        String title = ContextUtils.getApplicationContext().getString(titleId);
        Entry entry = getEntry(id);
        if (entry != null) {
            var builder = new Entry.Builder(entry).setTitle(title);
            if (targetFragment != null) builder.setFragment(targetFragment);
            updateEntry(id, builder.build());
        } else {
            throw new IllegalStateException("Existing ID cannot be found: " + id);
        }
    }

    /**
     * Updates the summary of the entry.
     *
     * @param prefFragment Full class name of the Fragment where the entry belongs.
     * @param key The name of the key for the preference entry.
     * @param summaryId String resource ID of the summary.
     * @throws IllegalStateException If a preference with the same key does not exist in the index.
     */
    public void updateEntrySummaryForKey(String prefFragment, String key, int summaryId) {
        String id = PreferenceParser.createUniqueId(prefFragment, key);
        var entry = getEntry(id);
        if (entry == null) {
            throw new IllegalStateException("Existing ID cannot be found: " + id);
        }
        String summary =
                summaryId != 0 ? ContextUtils.getApplicationContext().getString(summaryId) : null;
        updateEntry(id, new Entry.Builder(entry).setSummary(summary).build());
    }

    /**
     * Removes a preference entry from the index.
     *
     * <p>This method should be used when a link to a fragment is being hidden from one screen, but
     * the fragment itself is still reachable via another path and should remain searchable.
     *
     * <p>For example, when the "Appearance" setting is enabled, the top-level link to "Tabs" on the
     * main settings screen is removed, but the "Tabs" screen is still accessible through the
     * "Appearance" screen. In this case, the MainSettings provider should call this method to
     * remove only the redundant link, without disabling the {@code TabsSettings} fragment.
     *
     * @param id The unique ID of the preference link to remove.
     */
    public void removeEntry(String id) {
        assert PreferenceParser.isId(id) : "Use getUniqueId(key) to pass a unique id.";
        mEntries.remove(id);
        mRemovedKeys.add(id);
    }

    /**
     * Removes a preference entry of a given key from the index.
     *
     * @param prefFragment Full class name of the Fragment where the key belongs.
     * @param key Key name of the preference entry.
     */
    public void removeEntryForKey(String prefFragment, String key) {
        removeEntry(PreferenceParser.createUniqueId(prefFragment, key));
    }

    /** Set the flag indicating the index became stale and needs reindexing. */
    public void setNeedsIndexing() {
        sNeedsIndexing = true;
    }

    /**
     * Resets the flag indicating the index needs refreshing.
     *
     * <p>Ideally this method should only be used by the coordinator to reset the flag after the
     * index is initialized.
     */
    public void resetNeedsIndexing() {
        sNeedsIndexing = false;
    }

    /** Return whether the index data needs to be refreshed. */
    public boolean needsIndexing() {
        return sNeedsIndexing;
    }

    /** Set the flag indicating whether the search results fragment needs refreshing. */
    public void setRefreshResult(boolean refresh) {
        mShouldRefreshResult = refresh;
    }

    /** Returns whether whether the search results fragment needs refreshing. */
    public boolean shouldRefreshResult() {
        return mShouldRefreshResult;
    }

    /**
     * Clears all indexed entries and disabled fragments. This should be called before starting a
     * new indexing process.
     */
    public void clear() {
        mEntries.clear();
        mRemovedKeys.clear();
        mChildFragmentToParentKeys.clear();
        sNeedsIndexing = true;
    }

    /**
     * Registers a potential parent-child relationship between a preference and a fragment.
     *
     * @param childFragmentName The class name of the child fragment.
     * @param parentId The ID of the preference that links to the child fragment.
     */
    public void addChildParentLink(String childFragmentName, String parentId) {
        List<String> parents =
                mChildFragmentToParentKeys.computeIfAbsent(
                        childFragmentName, k -> new ArrayList<>());

        if (!parents.contains(parentId)) {
            parents.add(parentId);
        }
    }

    /**
     * Finalizes the index by resolving the correct header for each entry based on the currently
     * visible preferences and removes any orphaned entries that no longer have a valid parent path.
     *
     * @param rootFragmentName The class name of the root fragment (e.g., MainSettings).
     */
    public void resolveIndex(String rootFragmentName) {
        List<String> entriesToRemove = new ArrayList<>();

        for (Entry entry : mEntries.values()) {
            // Root entries have their own title as the header if they do not inherit one from the
            // XML.
            if (entry.parentFragment.equals(rootFragmentName)) {
                if (TextUtils.isEmpty(entry.header)) {
                    Entry updatedEntry = new Entry.Builder(entry).setHeader(entry.title).build();
                    updateEntry(entry.id, updatedEntry);
                }
                continue;
            }

            // Orphan Check (Pass null args for basic connectivity)
            List<Entry> path =
                    getBreadcrumbEntries(
                            entry.parentFragment, /* arguments= */ null, rootFragmentName);
            if (path == null && !entry.parentFragment.equals(rootFragmentName)) {
                entriesToRemove.add(entry.id);
                continue;
            }

            String header = (path == null || path.isEmpty()) ? null : path.get(0).title;

            boolean effectivelySearchable =
                    entry.isSearchable
                            && hasSearchablePathToRoot(entry.parentFragment, rootFragmentName);

            if (!TextUtils.equals(header, entry.header)
                    || entry.isSearchable != effectivelySearchable) {
                Entry updated =
                        new Entry.Builder(entry)
                                .setHeader(header)
                                .setIsSearchable(effectivelySearchable)
                                .build();
                updateEntry(entry.id, updated);
            }
        }

        for (String key : entriesToRemove) {
            removeEntry(key);
        }
    }

    /** Invokes {@link #resolveIndex} with MainSettings.class.getName(). */
    public void resolveIndex() {
        resolveIndex(sMainSettingsClassName);
    }

    private boolean hasSearchablePathToRoot(String fragmentName, String rootFragmentName) {
        if (fragmentName.equals(rootFragmentName)) return true;
        List<String> parentKeys = mChildFragmentToParentKeys.get(fragmentName);
        if (parentKeys == null) return false;

        for (String parentKey : parentKeys) {
            Entry parent = mEntries.get(parentKey);
            if (parent != null && parent.isSearchable) {
                if (hasSearchablePathToRoot(parent.parentFragment, rootFragmentName)) return true;
            }
        }
        return false;
    }

    /**
     * Calculates the shortest path from the root settings screen to the target fragment using a
     * Breadth-First Search traversal of the preference graph.
     *
     * <p>This method traverses upwards from the target fragment to the root. If a fragment is
     * reachable via multiple parents (e.g., {@code SingleCategorySettings} is used for Camera,
     * Microphone, Cookies, etc.), it resolves the ambiguity by evaluating the target's runtime
     * arguments against the preference key definitions in the index to select the correct parent.
     *
     * <p>The resulting path is utilized both for constructing interactive UI breadcrumbs on deep
     * links and for verifying graph connectivity when pruning orphans during index resolution.
     *
     * @param fragmentClass The full class name of the target fragment to calculate the path for.
     * @param arguments The runtime arguments (Bundle) of the target fragment. Used to identify the
     *     correct parent when a fragment class is reused across multiple preferences. Can be {@code
     *     null}.
     * @return A list of {@link Entry} objects representing the path, ordered from the top-level
     *     entry down to the immediate parent of the target. Returns {@code null} if no valid path
     *     to the root exists (i.e., the fragment is an orphan or its parent is disabled). Returns
     *     an empty list if the target is the root itself.
     */
    public @Nullable List<Entry> getBreadcrumbEntries(
            String fragmentClass, @Nullable Bundle arguments, String rootFragmentName) {
        if (rootFragmentName.equals(fragmentClass)) return new ArrayList<>();

        Queue<List<Entry>> queue = new ArrayDeque<>();
        Set<String> visited = new HashSet<>();
        visited.add(fragmentClass);

        List<String> immediateKeys = mChildFragmentToParentKeys.get(fragmentClass);
        if (immediateKeys == null) return null;

        List<Entry> validEntries = new ArrayList<>();
        for (String key : immediateKeys) {
            Entry entry = mEntries.get(key);
            if (entry != null && TextUtils.equals(entry.fragment, fragmentClass)) {
                validEntries.add(entry);
            }
        }

        if (validEntries.isEmpty()) return null;

        boolean isAmbiguous = validEntries.size() > 1;

        // Find the first matching parent
        for (Entry entry : validEntries) {
            if (isMatchingEntry(entry, arguments, isAmbiguous)) {
                List<Entry> path = new ArrayList<>();
                path.add(entry);
                queue.add(path);
            }
        }

        while (!queue.isEmpty()) {
            List<Entry> currentPath = queue.poll();
            Entry lastEntry = currentPath.get(currentPath.size() - 1);
            String parentFragment = lastEntry.parentFragment;

            if (rootFragmentName.equals(parentFragment)) {
                Collections.reverse(currentPath);
                return currentPath;
            }

            if (!visited.contains(parentFragment)) {
                visited.add(parentFragment);
                List<String> parentKeys = mChildFragmentToParentKeys.get(parentFragment);
                if (parentKeys != null) {
                    for (String parentKey : parentKeys) {
                        Entry parentEntry = mEntries.get(parentKey);
                        if (parentEntry != null) {
                            List<Entry> newPath = new ArrayList<>(currentPath);
                            newPath.add(parentEntry);
                            queue.add(newPath);
                        }
                    }
                }
            }
        }
        return null;
    }

    public @Nullable List<Entry> getBreadcrumbEntries(
            String fragmentClass, @Nullable Bundle arguments) {
        return getBreadcrumbEntries(fragmentClass, arguments, sMainSettingsClassName);
    }

    /**
     * Evaluates whether a candidate parent entry from the index matches the runtime arguments of
     * the target fragment.
     *
     * <p>This acts as a heuristic to disambiguate the correct parent when a single fragment class
     * is launched by multiple different preferences. It relies on the common Android Settings
     * pattern where the parent or the invoker injects its Preference Key into the fragment's
     * arguments bundle to dictate what content to display.
     *
     * @param indexEntry The candidate parent {@link Entry} being evaluated.
     * @param targetArgs The runtime arguments (Bundle) of the target fragment.
     * @param isAmbiguous {@code true} if the target fragment has multiple potential parents in the
     *     index, requiring strict key-value verification.
     * @return {@code true} if the entry is a valid parent for the given arguments, {@code false}
     *     otherwise.
     */
    private boolean isMatchingEntry(
            Entry indexEntry, @Nullable Bundle targetArgs, boolean isAmbiguous) {
        if (isAmbiguous) {
            // If targetArgs is null, we are performing a connectivity check during indexing
            // (resolveIndex) rather than resolving a specific deep link, so any valid parent path
            // is acceptable.
            if (targetArgs == null || targetArgs.isEmpty()) return true;

            String entryKey = indexEntry.key;
            if (entryKey != null) {
                for (String key : targetArgs.keySet()) {
                    Object value = targetArgs.getString(key);
                    // Match found: Entry Key ("camera") equals Argument Value ("camera")
                    if (value instanceof String && TextUtils.equals(entryKey, (String) value)) {
                        return true;
                    }
                }
            }
            return false;
        }

        return true;
    }

    /**
     * A container for the results of a search operation.
     *
     * <p>This class holds a list of {@link Entry} objects that matched a user's query. The items
     * are automatically sorted in descending order of relevance score upon being added.
     */
    public static class SearchResults {
        private final List<Map.Entry<Integer, Entry>> mScoredItems = new ArrayList<>();

        /** Return whether there was any search result. */
        public boolean isEmpty() {
            return mScoredItems.isEmpty();
        }

        /**
         * Add a matching entry to the result.
         *
         * @param item Matching entry to add.
         * @param score Matching score.
         */
        public void addItem(Entry item, int score) {
            // Add items in descending order of score.
            int i = 0;
            for (; i < mScoredItems.size(); ++i) {
                if (mScoredItems.get(i).getKey() < score) break;
            }
            mScoredItems.add(i, Map.entry(score, item));
        }

        /** Returns a list of search results. */
        public List<Entry> getItems() {
            List<Entry> entryList = new ArrayList<>();
            for (Map.Entry<Integer, Entry> pair : mScoredItems) {
                entryList.add(pair.getValue());
            }
            return entryList;
        }

        /** Returns a list of search results after grouping them by the header. */
        public ArrayList<Entry> groupByHeader() {
            Map<String, Integer> groups = new HashMap<>();
            ArrayList<Entry> results = new ArrayList<>();
            int pos = 0;

            // The input is already sorted by the score. Move up items till
            // they all get grouped together.
            for (Map.Entry<Integer, Entry> pair : mScoredItems) {
                Entry entry = pair.getValue();
                String header = entry.header;
                int groupPos = groups.getOrDefault(header, -1);
                if (groupPos < 0) {
                    // |groups| keep the position of the lowest entries of each group.
                    // The new item with the same group is inserted in that position.
                    groups.put(header, pos);
                    results.add(entry);
                } else {
                    // Push down all the items not in |header| and add the new one there.
                    if (groupPos == results.size() - 1) {
                        results.add(entry);
                    } else {
                        results.add(groupPos + 1, entry);
                    }
                    Map<String, Integer> newGroups = new HashMap<>();
                    for (String key : groups.keySet()) {
                        // Adjust |groups| after a new item is inserted. Any group
                        // below the current pos should be pushed down by one.
                        int p = groups.get(key);
                        newGroups.put(key, groupPos <= p ? p + 1 : p);
                    }
                    groups = newGroups;
                }
                ++pos;
            }
            return results;
        }
    }

    /**
     * Performs an in-memory search for the given query against all indexed preferences.
     *
     * <p>The search logic is as follows:
     *
     * <ol>
     *   <li>The user's query is normalized (converted to lowercase and diacritics are stripped).
     *   <li>The method iterates through all indexed {@link Entry} objects.
     *   <li>It performs a case-insensitive, diacritic-insensitive search against the normalized
     *       title and summary of each entry.
     *   <li>A scoring model is applied to rank results: an exact title match receives the highest
     *       score, followed by a partial title match, followed by a summary match.
     *   <li>An entry is added to the results list at most once, with the score of its best matching
     *       field.
     * </ol>
     *
     * @param query The user's search query. Can be null or empty.
     * @return A {@link SearchResults} object containing the list of matching {@link Entry} objects,
     *     sorted by relevance score. Returns an empty result for null or empty queries.
     */
    public SearchResults search(String query) {
        query = normalizeString(query);
        SearchResults results = new SearchResults();

        if (TextUtils.isEmpty(query)) {
            return results;
        }

        for (Entry entry : mEntries.values()) {
            if (!entry.isSearchable) continue;

            if (entry.mTitleNormalized != null && entry.mTitleNormalized.contains(query)) {
                int score =
                        TextUtils.equals(entry.mTitleNormalized, query)
                                ? EXACT_TITLE_MATCH
                                : PARTIAL_TITLE_MATCH;
                results.addItem(entry, score);
                continue;
            }

            if (entry.mSummaryNormalized != null && entry.mSummaryNormalized.contains(query)) {
                results.addItem(entry, PARTIAL_SUMMARY_MATCH);
                continue;
            }

            if (entry.keywords != null) {
                for (String keyword : entry.keywords) {
                    if (keyword != null && keyword.contains(query)) {
                        results.addItem(entry, PARTIAL_KEYWORD_MATCH);
                        break;
                    }
                }
            }
        }
        return results;
    }

    Map<String, Entry> getEntriesForTesting() {
        return mEntries;
    }

    /**
     * Returns a set of all original preference keys currently stored in the index. This is useful
     * for validating that the index matches the dynamically rendered UI.
     */
    public Set<String> getKeys() {
        Set<String> keys = new HashSet<>();
        for (Entry entry : mEntries.values()) {
            if (entry.key != null) {
                keys.add(entry.key);
            }
        }
        return keys;
    }

    /**
     * Returns a set of all unique IDs that were explicitly removed from the index. This is useful
     * for validating that a visible preference was intentionally excluded.
     */
    public Set<String> getRemovedKeys() {
        return mRemovedKeys;
    }

    Map<String, List<String>> getChildFragmentToParentKeysForTesting() {
        return mChildFragmentToParentKeys;
    }
}
