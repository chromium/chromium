# Welcome to components/visitedlink

This module contains the code that supports the in-memory hashtable storing
:visited link data.

**Have a problem to report?**: File a bug in [our component](https://issues.chromium.org/issues/new?component=1456589&template=0).

## We are currently under construction!

In this module, there are two code paths: the original unpartitioned :visited
link code and the experimental partitioned :visited link code. For our
motivations behind the partitioned :visited links experiment, please read the
[Partitioning :visited links history explainer](https://github.com/kyraseevers/Partitioning-visited-links-history).

The core concept of partitioned :visited links is to style anchor elements as
:visited if and only if they have been visited from this exact top-level site
and frame origin before. Functionally, this is represented by a triple key of:
`<link url, top-level site, frame origin>`.

Partitioned experiments are currently running on code gated behind either
`blink::features::kPartitionVisitedLinkDatabase` or
`blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks`. Files that are
prefixed with 'partitioned_' are only used by these experimental codepaths.

**HOWEVER:** all other files in this module are used in both the unpartitioned
and partitioned code paths. Changes to these files should consider the needs
of both models or appropriately gate behind the feature flags listed above.

## How do :visited links work?

A description of the design decisions and overview of the architecture for
unpartitioned :visited links can be found below in the Tradeoffs section.
Partitioned :visited links relies on many of the same tradeoff and design as
the original unpartitioned code.

### General Overview
Like many APIs within chromium code, functionality is split between the browser
and the renderer to ensure proper sandboxing. For both unpartitioned and
partitioned :visited links, we can broadly think of the browser code as "how we
store links" and the renderer code as "how we style links".

However, unlike a typical sandboxed API, :visited links must be extremely
performant due to the number of times it is called during the very time
sensitive page load process. As a result, we cannot rely on an asynchronous IPC
call between the renderer and browser every time we query a "style" to obtain
whether that link has been "stored".

The solution to this problem is to store the hashtable in memory, copy a
read-only shared memory handle to each RenderProcessHost, and send
asynchronous updates to each RPH table as necessary. The browser process
is the only process with write capabilities.

As a result, the renderer code still has quick access to the hashtable each time
it gets a "style" request, but it is still sandboxed from browser code and has
read-only access to the in-memory hashtable.

Partitioned :visited links improves upon this model further by introducing
"per-origin salts". For every origin the browser navigates to, we generate a
unique `uint64_t` [salt value](https://en.wikipedia.org/wiki/Salt_(cryptography)).
When adding a new link to the in-memory hashtable, we hash together all elements
of the triple key plus the unique salt value corresponding to that key's frame
origin. Then, when we encounter a new RenderProcessHost, we only send the salt
value corresponding to its origin. When querying the :visited-ness of a link
being loaded, the RenderProcessHost then provides that salt along with the other
elements of the link's triple key. Any queries with missing or invalid salt
values will return as unvisited.

As a result, a RenderProcessHost can only "read" or access :visited-ness
corresponding to the per-origin salt is has receivied. Therefore, in the
event of a renderer compromise, hashtable entries for all other origins are
"unreadable", as bad actors lack the relevant salts to query for other origins'
:visited data, and every cross-origin link queried will return an unvisited
status.

### Architecture

This module divides its code into subdirectories to achieve the sandboxing model
above and abide by layering constraints.
- `browser/`: contains code with write permissions to the in-memory hashtable;
  entrypoint for History codepaths.
- `renderer/`: contains code with read-only permissions to the in-memory
  hashtable; entrypoint for Blink codepaths.
- `common/`: contains code with read-only permissions to the in-memory
  hashtable, both (Partitioned)VisitedLinkWriter and VisitedLinkReader inherit
  and have access to code stored in common.
- `core/`: contains code which does not depend on `content/` and as such may be
  included in classes in this directory; can be accessed in codepaths used in
  iOS-builds, i.e. HistoryService, without encountering layering errors.
- `test/`: contains test code.

Code that does not require sandboxing lives in `common/`. Both
(Partitioned)VisitedLinkWriter (in `browser/`) and VisitedLinkReader
(in `renderer/`) inherit from the VisitedLinkCommon class. However, code in
`browser/` cannot depend on code in `renderer/` and vice versa. Data must be
sent via IPC (VisitedLinkNotificationSink) from browser to renderer.

Other classes of note inside `browser/` include VisitedLinkEventListener and
VisitedLinkUpdater which collate RenderProcessHosts and manage IPC, as well as,
VisitedLinkDelegate which allows us to build an in-memory hashtable from the
HistoryDatabase backend.

### Memory vs. Disk
Within `components/visitedlink/` the architecture of the in-memory hashtables
for both unpartitioned and partitioned links is very similar: an [open-
addressed hashtable](https://en.wikipedia.org/wiki/Open_addressing) containing
fingerprints generated by an MD5 hash. However, the two models diverge when it
comes to persisting the hashtable across browsing sessions.

The unpartitioned model stores its hashtable to a file on disk. It updates this
file asynchronously, and if uncorrupted at browser startup, will load the data
from this file into the in-memory table. In the event of corruption, the
hashtable is built from the URLDatabase table in the HistoryDatabase.

In contrast, the partitioned model has no additional file to which it saves the
table on disk. It relies on a new table, the VisitedLinkDatabase, to persist
this data across sessions. The in-memory hashtable is loaded from this database
at every browser startup.

## Tradeoffs

***NOTE: the below documentation was written years ago during the creation of
the original (unpartitioned) :visited link system. Some of these assumptions
may be out of date but provide important context and reasoning for why the
system makes many of these same tradeoffs today.***

### Background and motivation

Web browsers display links that the user has previously followed in a different
color than when they are unvisited. All web browsers before Chrome use the
history system for determining if a URL is visited. As the page is laid out, and
when the mouse hovers over a link, this system must be queried, so it is very
important that these lookups are fast (hundreds of links on a page is not
uncommon). Because this lookup is extremely performance critical, something like
a hashtable (Firefox 2) or Tree (Firefox 3) is used to speed queries.

Chrome had a design goal of being able to store all of a user's history forever.
This can easily be hundreds of thousands of URLs. Just storing them in memory
would be prohibitive, and we can not go to disk because of the performance
impact. Chrome has the additional problem that the rendering engine is in a
different process as the history system, requiring either an IPC message or a
duplication of data to get the information into the renderer.

### Unpartitioned Visited Link Architecture

Chrome separated the data store used for link coloring from the rest of history
to make it more scalable and performant. We do not store the full URL, but
rather 64 bits of the MD5 sum of the URL, and we store it in an open-addressed
hash table. Basically, we give up on being 100% correct in order to extremely
fast and memory-efficient. The odds of collisions are significantly below the
acceptable level for this application.

Because we only store 8 bytes per URL, we use very little memory and it can be
loaded quickly. The format of the open addressed hash table is the same in
memory as it is on disk, so it merely needs to be mapped into memory to be read.
The open addressed format is just a table, so there are no pointers or complex
data structures. This means that the renderers can map a read-only view of the
hash table and read it directly, without locking, even though another process is
changing it. In our scheme, only the browser process can write to the table. If
URL fingerprint (the 8-bit MD5 hash of it) is being added at the same time it is
being read, the read query will result in negative. This is fine because we do
not guarantee when the URL will be added to the table in the first place. This
collision does not affect queries of any other URL.

### Details

The browser and the renderer components are separate because only the browser
needs to have write capabilities. The common functions such as fingerprint
computation and hashtable lookup are provided by the `VisitedLinkCommon` class
in `components/visitedlink/common/visitedlink_common.h`. The renderer uses the
`VisitedLinkReader` object in
`components/visitedlink/renderer/visitedlink_reader.h` which just provides the
capability to receive the hashtable information from the browser, map it into
memory, and set it up for querying.

The browser uses the `VisitedLinkWriter` object in
`components/visitedlink/browser/visitedlink_writer.h`. This object is kept
up-to-date with the most recent changes in history by the `HistoryService`
object on the main thread of the browser. It will load the hash table from disk,
and provide the information for mapping it in the renderers

Sometimes the hashtable needs to be grown or shrunk. This is accomplished by
creating a new parallel hash table with the appropriate size and copying the
existing data to it. The `VisitedLinkWriter` releases its reference to the old
table and broadcasts the new table information to the renderers. The renderers
will continue using the old table, which is no longer being updated but still
exists, until they receive this message. The operating system will automatically
free the physical memory when the last renderer has released its shared memory
handle.

There is a slight security problem with the MD5 fingerprint approach. 64 bits is
not enough to be very cryptographically secure, so we need to assume that an
attacker can generate URLs that hash to any specific fingerprint. If an attacker
generates a page that hashes to the same URL as a popular site such as
google.com, and that link appears on an otherwise trusted site such as a Google
search result, the link may be colored as visited. This is a minor issue, but it
may make the user more likely to click on it or trust the site's content,
thinking they have been there before. To prevent this, we generate a salt when
creating the table. This means that all users will have different fingerprints
mapping to google.com, so an attacker does not know which URL to generate.

### I/O

The `VisitedLinkWriter` lives on the main thread, but we do not want to do I/O
on the main thread. For initialization, we just block the main thread. This
takes very little time, and we do not currently have a way to reevaluate the
status of all the links if it is loaded asynchronously, although this could be a
good future addition.

As mentioned above, the unpartitioned hashtable relies on a file stored on disk
independent of the HistoryDatabase to persist the hashtable across browsing
sessions or for backup in the event of memory corruptiion. When writing changes
to the file as the browser is running, it will send messages to the "file
thread" with the information. These 8-byte additions are then written
asynchronously to disk. When significant changes are made, the entire table is
copied to the background thread to avoid thousands of small messages.

Sometimes, we need to regenerate the hash table. We do this when we detect
corruption or when there is no visited link file. To do this, we collect all
URLs on a background thread using the `TableBuilder` object. When the collection
is done, we pass them to the main thread and insert them into the hash table.
This insertion is pretty quick. As noted above, the partitioned hashtable relies
on this process to persist across browsing sessions and regenerates from the
VisitedLinkDatabase each time at start-up.

