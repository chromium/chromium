// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

dictionary ReadingListEntry {
  // The url of the entry.
  required DOMString url;

  // The title of the entry.
  required DOMString title;

  // Will be <code>true</code> if the entry has been read.
  required boolean hasBeenRead;

  // The last time the entry was updated.
  // This value is in milliseconds since Jan 1, 1970.
  required double lastUpdateTime;

  // The time the entry was created.
  // Recorded in milliseconds since Jan 1, 1970.
  required double creationTime;
};

dictionary AddEntryOptions {
  // The url of the entry.
  required DOMString url;

  // The title of the entry.
  required DOMString title;

  // Will be <code>true</code> if the entry has been read.
  required boolean hasBeenRead;
};

dictionary RemoveOptions {
  // The url to remove.
  required DOMString url;
};

dictionary UpdateEntryOptions {
  // The url that will be updated.
  required DOMString url;

  // The new title. The existing tile remains if a value isn't provided.
  DOMString title;

  // The updated read status. The existing status remains if a value
  // isn't provided.
  boolean hasBeenRead;
};

dictionary QueryInfo {
  // A url to search for.
  DOMString url;

  // A title to search for.
  DOMString title;

  // Indicates whether to search for read (<code>true</code>) or unread
  // (<code>false</code>) items.
  boolean hasBeenRead;
};

// Listener callback for the onEntryAdded event.
// |entry|: The entry that was added.
callback OnEntryAddedListener = undefined (ReadingListEntry entry);

interface OnEntryAddedEvent : ExtensionEvent {
  static undefined addListener(OnEntryAddedListener listener);
  static undefined removeListener(OnEntryAddedListener listener);
  static boolean hasListener(OnEntryAddedListener listener);
};

// Listener callback for the onEntryRemoved event.
// |entry|: The entry that was removed.
callback OnEntryRemovedListener = undefined (ReadingListEntry entry);

interface OnEntryRemovedEvent : ExtensionEvent {
  static undefined addListener(OnEntryRemovedListener listener);
  static undefined removeListener(OnEntryRemovedListener listener);
  static boolean hasListener(OnEntryRemovedListener listener);
};

// Listener callback for the onEntryUpdated event.
// |entry|: The entry that was updated.
callback OnEntryUpdatedListener = undefined (ReadingListEntry entry);

interface OnEntryUpdatedEvent : ExtensionEvent {
  static undefined addListener(OnEntryUpdatedListener listener);
  static undefined removeListener(OnEntryUpdatedListener listener);
  static boolean hasListener(OnEntryUpdatedListener listener);
};

// Use the <code>chrome.readingList</code> API to read from and modify
// the items in the
// <a href="https://support.google.com/chrome/answer/7343019">Reading List</a>.
interface ReadingList {
  // Adds an entry to the reading list if it does not exist.
  // |entry|: The entry to add to the reading list.
  // |Returns|: Invoked once the entry has been added.
  static Promise<undefined> addEntry(AddEntryOptions entry);

  // Removes an entry from the reading list if it exists.
  // |info|: The entry to remove from the reading list.
  // |Returns|: Invoked once the entry has been removed.
  static Promise<undefined> removeEntry(RemoveOptions info);

  // Updates a reading list entry if it exists.
  // |info|: The entry to update.
  // |Returns|: Invoked once the matched entries have been updated.
  static Promise<undefined> updateEntry(UpdateEntryOptions info);

  // Retrieves all entries that match the <code>QueryInfo</code> properties.
  // Properties that are not provided will not be matched.
  // |info|: The properties to search for.
  // |Returns|: Invoked once the entries have been matched.
  // |PromiseValue|: entries
  [requiredCallback] static Promise<sequence<ReadingListEntry>> query(
      QueryInfo info);

  // Triggered when a <code>ReadingListEntry</code> is added to the reading
  // list.
  static attribute OnEntryAddedEvent onEntryAdded;

  // Triggered when a <code>ReadingListEntry</code> is removed from the reading
  // list.
  static attribute OnEntryRemovedEvent onEntryRemoved;

  // Triggered when a <code>ReadingListEntry</code> is updated in the reading
  // list.
  static attribute OnEntryUpdatedEvent onEntryUpdated;
};

partial interface Browser {
  static attribute ReadingList readingList;
};
