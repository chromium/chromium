# Desktop Omnibox Bug Triage Process

*last update: 2026/10/21*

*The current triage process owners are `jdonnelly@` and `avivkiss@`.*

Instructions for Chrome omnibox engineers serving an assigned bug triage
rotation.

[TOC]

## Goal of the process

Every new omnibox and search in Chrome bug is routed to the right team within
one business day of being filed. An automated triage agent does the first pass
(see [Automated triage](#automated-triage)). The triage engineer handles what
the agent can't or shouldn't decide, and reviews its work.

### Non-goals

Ensure high priority bugs or regressions are making progress / ping bug owners
for progress updates on other important bugs.  (Trust your teammates to work on
assigned bugs.)

## Process

### Quick reference

Component IDs:

 - 1457180: Chromium > UI > Browser > Omnibox
 - 1457335: Chromium > UI > Browser > Search

Hotlist IDs:

 - [Omnibox Core team inbox (8341471)](https://issues.chromium.org/hotlists/8341471):
   adding it routes the bug to Omnibox Core.
 - [Search Inputs & Suggest team inbox (8341082)](https://issues.chromium.org/hotlists/8341082):
   adding it routes the bug to Search Inputs & Suggest.
 - [Agent Triaged (9039956)](https://issues.chromium.org/hotlists/9039956):
   the triage agent adds it to every bug it looks at; don't add it yourself
   when you triage by hand.
 - [Needs Human (9040025)](https://issues.chromium.org/hotlists/9040025): the
   triage agent adds it when it couldn't route the bug; remove it once you've
   acted on the bug.
 - Legacy Available / Needs-* hotlists: see [Legacy process](#legacy-process)

### Automated triage

New bugs (type Bug or Feature Request, status New) in the public Omnibox and
Search components and their subcomponents are triaged automatically by a
public-mode Buganizer triage playbook (the triage agent). Public playbooks only
have Buganizer search and read tools and can only edit public data, so the
agent doesn't read code, add CCs, move bugs, or investigate. It adds the
[DS&I Agent Triaged](https://issues.chromium.org/hotlists/9039956) hotlist to
every bug it looks at. Whenever it changes a bug, it posts one short public
comment saying what it did and why. It adds the
[Needs Human](https://issues.chromium.org/hotlists/9040025) hotlist only when
it couldn't route the bug: it suggests a different component, or can't tell
which team owns it. A routed bug never gets Needs Human. For each bug it:

* **Skips** bugs that are already assigned, already in a team inbox, or parked
  in a legacy Needs-Feedback / Needs-TestConfirmation / Needs-Bisect hotlist.
* **Leaves mobile-only (Android/iOS) bugs alone**, unless the report clearly
  points at shared cross-platform code.
* **Handles out-of-scope bugs.** Reports about the google.com search box,
  Google Search or Google Lens on the web are closed as Won't Fix
  (Infeasible). For bugs owned by other teams (e.g. NTP, Lens in Chrome,
  security indicators, tabs), it comments with the suggested component and
  adds Needs Human so a person moves it.
* **Checks for duplicates.** It marks a bug as a duplicate only when it's very
  confident (same symptom and same trigger), and otherwise lists possibly
  related bugs. This covers the
  [commonly reported bugs and feature requests](#Commonly-referenced-bugs)
  listed below.
* **Routes** the bug to Omnibox Core or Search Inputs & Suggest by adding the
  team's inbox hotlist. It does not move the bug out of its component.
    * Omnibox Core: the omnibox and its popup (rendering, focus, editing,
      keyboard, accessibility), autocomplete infrastructure and providers,
      ranking, keyword mode / site search, and omnibox entry points.
    * Search Inputs & Suggest: suggestion content from Google (search
      suggestions, ZeroSuggest, Answers in Suggest, Drive suggestions, AI Mode
      suggestions), multimodal search inputs (context chips, voice, Lens in the
      input), AI Mode fulfillment, and the search box on surfaces outside the
      address bar.
    * WebUI omnibox: there are two WebUI implementations. If
      `WebUIOmniboxFullPopup` is on, Omnibox Core's version handles all input,
      so the bug goes to Omnibox Core. If only the WebUI toolbar's
      `WebUILocationBar` is on, the bug belongs to the WebUI toolbar team
      (Chromium > UI > Browser > WebUI Browser > Productionization). If
      neither is on, it is the classic omnibox.
* **Checks priority** against the Chrome bug SLOs
  ([go/chrome-slo](https://goto.google.com/chrome-slo)) and fixes the type of
  feature requests filed as bugs. It does not assign bugs.

The agent can't see bugs with an issue access limit (e.g.
Restrict-View-Google). Humans triage those: route the bug by adding the
[Omnibox Core](https://issues.chromium.org/hotlists/8341471) or
[Search Inputs & Suggest](https://issues.chromium.org/hotlists/8341082) team
inbox hotlist, or move or assign it. If the restriction isn't needed, remove
it so the agent can pick the bug up.

### Daily process

Every weekday at 5 AM Pacific, the triage engineer (the primary on the Omnibox
Bug Triage rotation) gets an email digest of the triage agent's work. The
rotation hands off on Mondays, and Monday's digest goes to the new triage
engineer. The digest covers the last 24 hours; Monday's covers the delta from Friday's digest to the generation of Monday's digest.
Each section says how it was built and what to do. Work through the sections
in order, using the rules above for anything you triage by hand:

* **Needs Human**: every open bug on the
  [Needs Human](https://issues.chromium.org/hotlists/9040025) hotlist,
  whatever its age. Read the agent's comment (a suggestion; you decide), then
  route, move or close the bug. Then remove it from the Needs Human hotlist so
  it drops off the list.
* **Restricted - agent cannot access**: untriaged bugs with an access limit
  that hides them from the agent (see above). A bug stays on this list until
  someone triages it by hand.
* **Agent could not finish**: untriaged bugs where the agent left an update
  but didn't finish (it never added the
  [DS&I Agent Triaged](https://issues.chromium.org/hotlists/9039956) hotlist).
  Read its update and finish the triage: route, move or assign the bug.
* **Not picked up by the agent**: untriaged bugs filed more than two hours
  ago that the agent hasn't looked at. Triage them by hand if they matter now.
  If bugs keep showing up here, tell `avivkiss@`: the agent may be stuck.
* **Keyword sweep**: new, unassigned bugs in other Chromium components that
  mention the omnibox, omnibar, location bar or address bar, each with a guess
  at whether it's ours. Move the ones that are ours to the Omnibox or Search
  component, and the agent triages them from there. Ignore the rest. This
  replaces the weekly check of the
  [Relevant Non-Omnibox Untriaged Bugs](https://issues.chromium.org/savedsearches/6740778)
  saved search.
* **Routed**: how many bugs the agent routed to each team, and a bug-by-bug
  list of the bigger changes it made (closures, duplicates, priority or type
  changes). Bugs waiting in Needs Human aren't repeated here. Nothing is
  required: spot-check the listed changes and fix anything that looks wrong.

Mobile-only (Android/iOS) bugs are left out of the digest; the mobile teams
triage those.

See the
[internal DS&I triage docs](https://g3doc.corp.google.com/chrome/search/dsi_engineering_docs/g3doc/bug_triage.md)
for more on the digest, including how to set up your own copy. If you're on
call and don't get it, contact `avivkiss@`.

### Weekly process

* Every week on Thursday, the triage engineer looks over all alerts sent to
  [chrome-omnibox-team-alerts@](https://groups.google.com/a/google.com/forum/#!forum/chrome-omnibox-team-alerts)
  and, for each, either files a bug or replies to the message indicating why
  filing a bug is not appropriate.  These bugs should be set to "Untriaged",
  so that the current triage engineer sees them, until a root cause has been
  identified and an owner assigned (or closed.) More details available
  [below](#How-to-triage-alerts).

Other team members are welcome to triage a bug if they see it before the triage
engineer.  The triage engineer will cycle among team members by arrangement.

### Legacy process

Before automated triage, triage engineers parked bugs in these hotlists. They
are being retired, and the daily digest doesn't cover them. Don't add new bugs
to them.

The hotlists are shared across Chromium, so each link below is filtered to
bugs in the Omnibox component (1457180, excluding SecurityIndicators 1456767)
and the Search component (1457335), including their subcomponents.

 - [Available (5438642)][legacy-available]
 - [Needs-Bisect (5432513)][legacy-needs-bisect]: someone needs to find the
   change that caused the bug.
 - [Needs-Feedback (5433459)][legacy-needs-feedback]: the bug needs more
   information from the reporter or someone outside the team.
 - [Needs-TestConfirmation (5432926)][legacy-needs-testconfirmation]: someone
   needs to reproduce the bug.

[legacy-available]: https://issues.chromium.org/issues?q=hotlistid:5438642%20(componentid:1457180%2B%20%7C%20componentid:1457335%2B)%20-componentid:1456767%2B
[legacy-needs-bisect]: https://issues.chromium.org/issues?q=hotlistid:5432513%20(componentid:1457180%2B%20%7C%20componentid:1457335%2B)%20-componentid:1456767%2B
[legacy-needs-feedback]: https://issues.chromium.org/issues?q=hotlistid:5433459%20(componentid:1457180%2B%20%7C%20componentid:1457335%2B)%20-componentid:1456767%2B
[legacy-needs-testconfirmation]: https://issues.chromium.org/issues?q=hotlistid:5432926%20(componentid:1457180%2B%20%7C%20componentid:1457335%2B)%20-componentid:1456767%2B

## How to triage chromium bugs

The triage engineer's job is to oversee routing: every bug should reach the
team that owns it. When you triage a bug by hand, read the report and do your
best to pick the right team: add the Omnibox Core or Search Inputs & Suggest
team inbox hotlist, or move the bug to the component that owns it. The owning
team takes it from there, including reproducing it, setting its priority and
owner, and following up with the reporter (for example, asking for
chrome://omnibox-internals data; see the
[example request](#Example-request-for-chrome_omnibox_internals-data) below).

If something about a bug you triage by hand is obviously off, such as its
priority or type, you're empowered to fix it.

### Priority

Follow [standard Chromium
policies](https://www.chromium.org/for-testers/bug-reporting-guidelines/triage-best-practices).
*Priority-2* represents wanted for this release but can be punted for a release.
*Priority-3* are bugs not time sensitive.

If you aren't sure of the scope, severity, or implications of an issue, prefer
to assign it a higher priority (*1* or *2*) and try to assign it to someone
appropriate to look into further or at least identify the scope / true priority.
If you cannot identify a person to assign it to, it would be better to leave it
*Untriaged* for the next triage engineer than mark it *Available* just to try to
clear the queue.

### Subcomponents

The subcomponents of omnibox bugs include:

| Subcomponent | Description |
| --- | --- |
| UI>Browser>Omnibox>AiS | Answers in Suggest. |
| UI>Browser>Omnibox>DocumentSuggest | Documents provided by Google Drive |
| UI>Browser>Omnibox>NTPRealbox | Suggestions displayed in the searchbox on the New Tab Page. |
| UI>Browser>Omnibox>SecurityIndicators | Secure/insecure icons; triaged by another team. |
| UI>Browser>Omnibox>TabToSearch | Custom search engines, omnibox extensions, etc. (including adding, triggering, ranking, etc. for them). |
| UI>Browser>Omnibox>ZeroSuggest | Suggestions displayed on omnibox focus (both contextual and non-contextual). |

## How to triage alerts

Every message sent to
[chrome-omnibox-team-alerts@](https://groups.google.com/a/google.com/forum/#!forum/chrome-omnibox-team-alerts)
should be evaluated by a triage engineer.  The triage engineer should either

* file a Chromium bug to investigate the issue further and reply to the alert
  with a link to the Chromium bug, or
* reply to the alert explaining that the alert is already being tracked, and
  link to the appropriate Chromium bug, or
* reply to the alert explaining why ignoring the alert is appropriate.  This
  is only appropriate if the engineer believes the alert is incorrect or
  spurious.

> **Tip**: Alerts on Beta and Stable that happen at the time of a new version
> release will often have triggered an alert on Dev.  Look for those Dev alerts.
> Most likely they already have associated bugs.  If the Beta/Stable change is
> the same scale at the Dev change, it's likely the same issue--you should
> simply point the Beta/Stable alert thread to the existing bug thread.

With this process, when the next triage engineer begins their triage rotation,
they'll be able to see which alerts have been handled and which have not.  All
alerts that have been handled will have a reply from a triage engineer.  All
alerts that have not been looked at will not.

Sometimes multiple alerts will be fired at around the same time, on the same
channel, on related histograms.  All of these alerts can be filed as part of the
same bug if the triage engineer thinks they're likely all related (as they
likely are).  All these alerts should be mentioned in the bug and all the
separate alert threads should be replied to pointing to that bug.

When filing the bug about an alert:

* Leave it *Untriaged*.  You should investigate it during your triage shift;
  if you don't identify a root cause, the next triager will see the bug and try.
  Only assign an owner if the likely root cause has been determined!
* Add the Restrict-View-Google label so metrics can be discussed without fear
  of leaking sensitive information.
* Link to the alert message.
* Set a priority.  Generally these bugs should be *Priority-1* or *Priority-2*.
* Tag it with a milestone (if appropriate).
* Likely, label it with either Performance-Browser or Hotlist-OmniboxRanking.
  (Probably one of those is appropriate.)

### Investigating an Alert

The [timeline dashboard](https://goto.google.com/uma-timeline) is your friend,
especially the split by channel, split by platform, split by milestone, and
split by version features.  Some tips on how to investigate using the timeline
dashboard:

* If the regression is on Dev, see if you can spot it on Canary.  That can
  usually indicate a narrow regression range.  This can usually be done unless
  the histogram is too noisy.
* What platforms did the regression happen on?  That might narrow down the area
  of relevant code.
* Did the regression happen on multiple channels at the same time?  If so, then
  it's either caused by a change that was submitted and quickly merged to
  multiple channels or it may be caused by something external to Chrome (such as
  a major holiday or significant weather events, which can change omnibox
  behavior).

**Action**: The best way to verify that a particular changelist caused a
regression is to **revert the changelist** and see if the metrics improve.  This
is a better strategy than landing a fix directly.  If the "fix" doesn't make the
metric recover to the exact same degree as the regression, it's unclear whether
the fix was related to the regression, only a partial solution, and whether
there's still another issue.

FYI: the alerting system does not alert on ChromeOS changes because the vast
majority of alerts that trigger on ChromeOS are due to school schedules (e.g.,
summer and winter vacation).  For the year prior to disabling ChromeOS alerts,
we did not see a real ChromeOS alert that wasn't also fired on Windows.  (I.e.,
all real regressions on ChromeOS were also regressions on Windows; Views
platforms tend to regress simultaneously.)  Thus, we seemingly don't get any
additional value from ChromeOS alerts.


# Appendix

## Example request for chrome://omnibox-internals data

NOTE: If you ask someone for chrome://omnibox-internals data on a public bug,
label the bug with Restrict-View-Google so that any personal data from the
reporter's chrome://omnibox-internals output is not made public. Do this
*before* they respond.
As the original reporter, they should still have access to the bug even with the
restrict applied.

Example request:

> Please visit chrome://omnibox-internals in the version of Chrome in which
> you're experiencing the issue and type the input that triggers the issue into
> the "Enter omnibox input" text box. Then click the Download link and attach
> the downloaded file to this issue.
>
> Please be aware that this data may reveal details of your browsing history so
> only attach the file if you're comfortable sharing that data with the omnibox
> team. I have added the Restrict-View-Google label to this issue so that it is
> not visible to the public.

## Commonly referenced bugs

* “I want the option to turn off inline autocomplete.” Dup against
  [crbug/91378](https://bugs.chromium.org/p/chromium/issues/detail?id=91378).

  * Try to understand the motivation of the user making the request.  Please
  ask the user for examples, with chrome://omnibox-internals detail (see above),
  of times where the omnibox doesn’t do what they want.  Ideally we should be
  able make to make the omnibox smart enough that such a feature isn’t
  necessary.

* “I want to disable suggestions from appearing entirely”. Dup against [crbug/1470391](https://bugs.chromium.org/p/chromium/issues/detail?id=1470391)

  * Try to understand the motivation of the user making the request. Consider
  quoting from [pkasting’s comment on this bug](https://bugs.chromium.org/p/chromium/issues/detail?id=702850#c16).

* “I typed in something like go/foo and got redirected to a search results page
  instead.” See
  [this internal page](https://docs.google.com/document/d/140jmrHfC9BiNUbHEmUF4ajJ8Zpbc7qd5fjTBulH3I5g/edit#).
  Follow the guidelines there; generally ask about a message about profile
  corruption.  If so, the bug can be dupped against
  [crbug/665253](https://bugs.chromium.org/p/chromium/issues/detail?id=665253)

  * … and no “Did you mean?” infobar appears.  This is likely prerendering; see
  [crbug/247848](https://bugs.chromium.org/p/chromium/issues/detail?id=247848)

# References

[General Chromium bug triage guidelines](https://www.chromium.org/getting-involved/bug-triage)

[Omnibox bugs that we intend/hope to tackle this year](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20NextAction%3C2018/1/1%20OR%20component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20-has:NextAction%20&sort=pri&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified),
broken down:

* [User-facing](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20-Hotlist=CodeHealth%20-Hotlist=Refactoring%20-component:Test%20NextAction%3C2018/1/1%20OR%20component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20-Hotlist=CodeHealth%20-Hotlist=Refactoring%20-component:Test%20-has:NextAction&sort=pri&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified)
  (everything not tagged as one of the non-user-facing categories below).  Some
  of these can be further categorized: Performance, Polish, Enterprise, Answers
  in Suggest, Tab To Search, Zero Suggest.

* Non user-facing, divided into these categories:

  * [Code health](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20NextAction%3C2018/1/1%20OR%20component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20-has:NextAction%20&sort=pri&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified)

  * [Refactoring](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20Hotlist=Refactoring%20NextAction%3C2018/1/1%20OR%20component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20Hotlist=Refactoring%20-has:NextAction&sort=pri&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified)

  * [Testing](https://bugs.chromium.org/p/chromium/issues/list?can=1&q=component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20component:Tests%20NextAction%3C2018/1/1%20OR%20component:UI%3EBrowser%3EOmnibox%20-component:UI%3EBrowser%3EOmnibox%3ESecurityIndicators%20status:Available,Assigned,Started%20component:Tests%20-has:NextAction&sort=pri&colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified)
