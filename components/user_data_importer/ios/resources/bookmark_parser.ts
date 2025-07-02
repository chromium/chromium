// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Parses the currently-loaded page according to the Netscape
 * bookmark format:
 *   https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/aa753582(v=vs.85)
 * Returns the parsed data synchronously.
 *
 * NOTE: This file is a utility that's run in a special WKWebView. It is not
 * bundled/injected into normal browser tabs and cannot be called in them.
 *
 */

// Corresponds to C++ class of the same name.
class ImportedBookmarkEntry {
  isFolder: boolean = false;
  url?: string;
  path: string[] = [];
  title?: string;
  creationTime?: number;  // Seconds from UNIX epoch
}

// Corresponds to C++ class of the same name.
class ParsedBookmarks {
  bookmarks: ImportedBookmarkEntry[] = [];
  readingList: ImportedBookmarkEntry[] = [];
}

// Tree representation of a bookmark, used internally only.
class Bookmark {
  name?: string;
  creationTime?: number;  // Seconds from UNIX epoch
  url?: string;
}

// Tree representation of a folder, used internally only.
class Folder {
  name?: string;
  creationTime?: number;  // Seconds from UNIX epoch
  isReadingList: boolean = false;
  children: BookmarkNode[] = [];
}

type BookmarkNode = Bookmark|Folder;

// Returns the child elements of `elt` with tagname `string`. `string` should be
// upper-case (e.g., "DT" not "dt").
function getFirstLevelChildrenMatchingTag(
    elt: Element, tag: string): Element[] {
  return Array.from(elt.children).filter((child) => child.tagName === tag);
}

// Transforms an A element into a Bookmark. Returns null if the tag can't be
// interpreted as a bookmark (e.g., no URL present).
function processA(a: HTMLElement): Bookmark|null {
  const bookmark = new Bookmark();
  const url = a.getAttribute('HREF');
  if (!url) {
    return null;
  }
  bookmark.url = url;

  if (a.innerText) {
    bookmark.name = a.innerText;
  }

  const creationTime = Number(a.getAttribute('ADD_DATE'));
  if (creationTime) {
    bookmark.creationTime = creationTime;
  }
  return bookmark;
}

// Transforms an H3 element (representing metadata) and a DL element
// (representing a list of bookmarks or subfolders) into a Folder object. May
// return an empty folder.
function processH3AndDL(h3: HTMLElement, dl: Element): Folder {
  const folder = new Folder();
  if (h3.innerText) {
    folder.name = h3.innerText;
  }

  const creationTime = Number(h3.getAttribute('ADD_DATE'));
  if (creationTime) {
    folder.creationTime = creationTime;
  }

  folder.isReadingList = (h3.id === 'com.apple.ReadingList');

  const dts = getFirstLevelChildrenMatchingTag(dl, 'DT');

  for (const dt of dts) {
    const child = processDt(dt);
    if (child) {
      folder.children.push(child);
    }
  }

  return folder;
}

// Transforms a DT element into either a Bookmark or a Folder, depending on its
// contents. Returns null if the contents aren't valid or don't correspond to a
// supported type.
function processDt(dt: Element): BookmarkNode|null {
  // There are two recognized formats inside a DT:
  // 1. A bookmark starts with an A tag.
  // 2. A folder starts with an H3 then has a DL tag.
  const firstChild = dt.firstElementChild;

  switch (firstChild?.tagName) {
    case 'A':
      return processA(firstChild as HTMLElement);
    case 'H3':
      return dt.children[1]?.tagName === 'DL' ?
          processH3AndDL(firstChild as HTMLElement, dt.children[1]) :
          null;
  }

  return null;
}

// Extracts the bookmarks and folders from the page and represents them as a
// tree (where the root node is implicit and its first-level children are stored
// in the returned array). May return an empty array if no valid bookmarks or
// folders can be extracted.
function treeify(): BookmarkNode[] {
  const nodes: BookmarkNode[] = [];

  // The documented format has the entire list encapsulated in a <DL> at the
  // top level. Real-world Safari examples have the DTs as top-level children
  // of document.body. Prefer the DL if present.
  const root =
      getFirstLevelChildrenMatchingTag(document.body, 'DL')[0] ?? document.body;

  const dts = getFirstLevelChildrenMatchingTag(root, 'DT');
  for (const dt of dts) {
    const node = processDt(dt);
    if (node) {
      nodes.push(node);
    }
  }

  return nodes;
}

function isReadingList(node: BookmarkNode): boolean {
  return node instanceof Folder ? node.isReadingList : false;
}

function linearize(
    node: BookmarkNode, path: string[]): ImportedBookmarkEntry[] {
  const result = new ImportedBookmarkEntry();
  result.title = node.name;
  result.creationTime = node.creationTime;
  result.path = path.slice();

  if (node instanceof Bookmark) {
    result.isFolder = false;
    result.url = node.url;
    return [result];
  }

  result.isFolder = true;

  // Append the current node's title to a copy of `path`. Use a copy to avoid
  // mutating state higher in the stack.
  const pathToHere = path.slice();
  pathToHere.push(result.title ?? '');

  // Recursively linearize each of the children of this node. Flatten the
  // result into a single array (the hierarchy is preserved by the `path`
  // values).
  const children: ImportedBookmarkEntry[] =
      node.children.map((node) => linearize(node, pathToHere)).flat();

  // Insert the folder before its children and return the result.
  children.unshift(result);
  return children;
}

function parse(): ParsedBookmarks {
  const nodes: BookmarkNode[] = treeify();

  const parsed = new ParsedBookmarks();

  // Filter evaluates to `node.isReadingList` if node is a Folder, and
  // undefined (falsy) if it's a Bookmark.
  const readingList: BookmarkNode[] = nodes.filter(isReadingList);
  const notReadingList: BookmarkNode[] =
      nodes.filter((node) => !isReadingList(node));

  // Transform the trees into the linear format required by the importer.
  parsed.readingList = readingList.map((node) => linearize(node, [])).flat();
  parsed.bookmarks = notReadingList.map((node) => linearize(node, [])).flat();

  return parsed;
}

// IOSBookmarkParser Obj-C++ class will concatenate `return parsed;` prior to
// injection. WKWebView accepts only a function body, but the TS compiler
// complains about `return` appearing outside a function, so we have to add it
// at runtime after TS->JS transpilation.
// eslint-disable-next-line @typescript-eslint/no-unused-vars
const parsed = parse();
