# Omnibox Bug Triage Process

*last update: 2018/06/11*

*The current triage process owner is `mpearson@`.*

Instructions for Chrome omnibox engineers serving an assigned bug triage
rotation.

[TOC]

## Goal of the process

Every omnibox bug is triaged, preferably within a week of being filed.  This
means making the issue clear, marking it as a dup if necessary, revising the
status, setting a priority, labeling/categorizing as appropriate (or handing off
to another component), and (possibly) assigning an owner.  The only reason bugs
should remain untriaged (i.e., *status=Unconfirmed* or *status=Untriaged*) is
because of they need additional information from someone.  These bugs should be
labeled as such (*Needs=Feedback* or *Needs=TestConfirmation*).

### Non-goals

* Ensure high priority bugs or regressions are making progress / ping bug owners
  for progress updates on other important bugs.  (Trust your teammates to work
  on assigned bugs.)

## Process

* Every other day (weekend excluded), the triage engineer looks over [all
  *Unconfirmed* and *Untriaged* omnibox bugs (without *Needs=Feedback*)](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-component%3AUI%3EBrowser%3EOmnibox%3ESecurityIndicators+-Needs%3DFeedback&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
  and triages them.
* Every week on Tuesday, the triage engineer looks over [all *Unconfirmed* and
  *Untriaged* bugs filed that aren’t categorized as omnibox yet have relevant
  terms](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=omnibox+-component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-has%3Aowner+OR+%E2%80%9Comni+box%E2%80%9D+-component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-has%3Aowner+OR+omnibar+-component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-has%3Aowner+OR+%E2%80%9Comni+bar%E2%80%9D+-component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-has%3Aowner+OR+locationbar+-component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-has%3Aowner+OR+%E2%80%9Clocation+bar%E2%80%9D+-component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-has%3Aowner+OR+addressbar+-component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-has%3Aowner+OR+%E2%80%9Caddress+bar%E2%80%9D+-component%3AUI%3EBrowser%3EOmnibox+status%3AUnconfirmed%2CUntriaged+-has%3Aowner&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
  to see if any should be moved to the omnibox component and triaged.  ([This
  scan can be limited to those filed in the last week, i.e., since the last
  check.](https://bugs.chromium.org/p/chromium/issues/list?colspec=ID%20Pri%20M%20Stars%20ReleaseBlock%20Component%20Status%20Owner%20Summary%20OS%20Modified&x=m&y=releaseblock&cells=ids&q=omnibox%20-component%3AUI>Browser>Omnibox%20status%3AUnconfirmed%2CUntriaged%20-has%3Aowner%20modified>today-14%20OR%20“omni%20box”%20-component%3AUI>Browser>Omnibox%20status%3AUnconfirmed%2CUntriaged%20-has%3Aowner%20modified>today-14%20OR%20omnibar%20-component%3AUI>Browser>Omnibox%20status%3AUnconfirmed%2CUntriaged%20-has%3Aowner%20modified>today-14%20OR%20“omni%20bar”%20-component%3AUI>Browser>Omnibox%20status%3AUnconfirmed%2CUntriaged%20-has%3Aowner%20modified>today-14%20OR%20locationbar%20-component%3AUI>Browser>Omnibox%20status%3AUnconfirmed%2CUntriaged%20-has%3Aowner%20modified>today-14%20OR%20“location%20bar”%20-component%3AUI>Browser>Omnibox%20status%3AUnconfirmed%2CUntriaged%20-has%3Aowner%20modified>today-14%20OR%20addressbar%20-component%3AUI>Browser>Omnibox%20status%3AUnconfirmed%2CUntriaged%20-has%3Aowner%20modified>today-14%20OR%20“address%20bar”%20-component%3AUI>Browser>Omnibox%20status%3AUnconfirmed%2CUntriaged%20-has%3Aowner%20modified>today-14&can=2))
* Every week on Thursday, the triage engineer looks over all alerts sent to
  [chrome-omnibox-team-alerts@](https://groups.google.com/a/google.com/forum/#!forum/chrome-omnibox-team-alerts)
  and, for each, either files a bug or replies to the message indicating why
  filing a bug is not appropriate.  These bugs should be set to "Untriaged",
  so that the current triage engineer sees them, until a root cause has been
  identified and an owner assigned (or closed.) More details available
  [below](#How-to-triage-alerts).
* Every week on Thursday, the triage engineer should
  look over [all bugs with *Needs=Feedback*](https://bugs.chromium.org/p/chromium/issues/list?can=2&q=component%3AUI%3EBrowser%3EOmnibox+Needs%3DFeedback+&colspec=ID+Pri+M+Stars+ReleaseBlock+Component+Status+Owner+Summary+OS+Modified&x=m&y=releaseblock&cells=ids)
  and should take action on those that have been sitting for too long.
  * If there's been no feedback over a week since the label was added, ping the
    reporter (or whoever's being asked for feedback) and politely ask them to
    provide feedback.
  * If there's been no feedback for over a week since the last ping, and no one
    can reproduce the issue, close it as WontFix.

Other team members are welcome to triage a bug if they see it before the the
triage engineer.  The triager owner will cycle among team members by
arrangement.

## How to triage chromium bugs

### Purpose

The purpose of labels, components, and priority are to make it easy for omnibox
team members to identify bugs at a glance that affect a particular area.  In
other words, this bug management process is intended to make the team more
efficient and provide better visibility for team members to identify what’s
important to work on in particular areas.

### Identifying the issue

If the bug isn’t clear, or the desired outcome is unclear, please apply the
label *Needs-Feedback* and courteously ask for clarification.  This is
appropriate both for external bug filers, Chromium developers on other teams,
or Googlers.  The label is merely an indication that this bug needs information
from someone outside the team in order to make progress.  It’s also an
indication that someone on the team needs to follow-up if feedback is not
forthcoming.

Often the appropriate request includes a request for chrome://omnibox data;
[an example such request](#Example-request-for-chrome_omnibox-data) is below.

Also, if the bug is clear, try to reproduce.  If it cannot be reproduced or you
lack the necessary setup, either ask the reporter for clarification or add the
label *Needs-TestConfirmation* as appropriate.

Some bugs are feature requests.  Marks these as Feature, not Bug.  It’s likely a
feature may have been requested before.  Search the bugs database and dup
appropriately.

Some [commonly requested features and
commonly reported bugs](#Commonly-referenced-bugs) are listed below.

### Status

Set the Status field to the following value depending on the triage action
taken:

* *Available*: The typical state for bugs exiting the triage process, assuming
  none of the other states below apply.
* *Assigned*: *Available* bugs with Owners; see
  [section on assigning owners](#Owners).
* *ExternalDependency*: Bugs that turn out to be in another project's code and
  you've contacted that other project.  Typically this is problems with
  Google-sourced suggestions, an IME, or a Chrome extension.  In all cases,
  file a bug or contact the appropriate team and post information about this
  outreach (e.g., a bug number) in a comment on the Chromium bug.
* *Duplicate*: This issue has been reported in another bug or shares the same
  root cause as another bug and will certainly be fixed simultaneously.
* *WontFix*: Anything working by design.  Also applies to old non-reproducible
  bugs or bugs lacking feedback per
  [standard Chromium policies](http://www.chromium.org/getting-involved/bug-triage#TOC-Cleaning-up-old-bugs).
* *Unconfirmed* or *Untriaged*:  These labels are generally only appropriate
  if you labeled the bug with *Needs-Feedback* or *Needs-TestConfirmation*,
  otherwise in effect you're kicking the can down the road to the next triage
  engineer.

### Priority

Follow [standard Chromium
policies](https://www.chromium.org/for-testers/bug-reporting-guidelines/triage-best-practices).
*Priority-2* represents wanted for this release but can be punted for a release.
*Priority-3* are bugs not time sensitive.  There is an even-lower-priority
state; see the *NextAction=01/07/2020* below.

If you aren't sure of the scope, severity, or implications of an issue, prefer
to assign it a higher priority (*1* or *2*) and try to assign it to someone
appropriate to look into further or at least identify the scope / true priority.
If you cannot identify a person to assign it to, it would be better to leave it
*Untriaged* for the next triage engineer than mark it *Available* just to try to
clear the queue.

### Owners

Generally only assign someone else to own a bug if the bug is *Priority-2* or
better.  *Priority-3* bugs are not likely to be completed soon; for these bugs,
don’t assign an owner to merely sit on the bug.  To draw someone’s attention to
a bug, explicitly CC them.

### Categorization

Omnibox bugs use a combination of labels (including hotlists) and subcomponents
to indicate their topic.

Most omnibox bugs should be in the main omnibox component.  A few will fall
directly into a subcomponent instead; the available subcomponents are listed
below.

Add all the labels, components, and next actions dates that seem appropriate to
the issue.  The main ones encountered by omnibox issues are listed below; feel
free to use additional suitable ones as well.

The main labels that apply to bugs include:

| Label | Description |
| --- | --- |
| Hotlist-OmniboxFocus | Bugs about focus, including not having focus when it should, focussing on the wrong end of the URL, etc. (Note: label does not appear in the Hotlist completion dropdown.) |
| Hotlist-OmniboxKeyboardShortcuts | Bugs related to handling of keyboard shortcuts, including both incorrectly handling real omnibox shortcuts and preventing other Chrome shortcuts from working when focus is in the omnibox. (Note: label does not appear in the Hotlist completion dropdown.) |
| Hotlist-OmniboxRanking | Bugs related to which suggestions are considered matches against the input and how relevance scores are assigned. (Note: label does not appear in the Hotlist completion dropdown.) |
| Hotlist-CodeHealth | Bugs about the making the code base easy to work with such as quality of comments.  Refactoring efforts can also be included when for that purpose. |
| Hotlist-Polish | Bugs about how a feature looks and feels: not whether the feature works or not; instead, more of whether it appears the feature was created with attention to detail.  Often user-visible edge cases fit in this category. |
| Hotlist-Refactoring | Bugs related to restructuring existing code without changing its behavior. |
| Performance-Browser | Bugs that affect performance. |

The components that additionally apply to bugs include:

| Component | Description |
| --- | --- |
| Enterprise | Bugs that mainly affect enterprise environments: enterprise policies, intranet handling, etc. |
| Tests>{Disabled,Failed,Missing,Flaky} | Bugs related to failing tests or lack of test coverage. |
| UI>Browser>Search | Bugs related to web search in general whose effects aren’t limited to the omnibox. |

The subcomponents of omnibox bugs include:

| Subcomponent | Description |
| --- | --- |
| UI>Browser>Omnibox>AiS | Answers in Suggest. |
| UI>Browser>Omnibox>DocumentSuggest | Documents provided by Google Drive |
| UI>Browser>Omnibox>SecurityIndicators | Secure/insecure icons; triaged by another team. |
| UI>Browser>Omnibox>TabToSearch | Custom search engines, omnibox extensions, etc. (including adding, triggering, ranking, etc. for them). |
| UI>Browser>Omnibox>ZeroSuggest | Suggestions displayed on omnibox focus (both contextual and non-contextual). |

If the bug is extremely low priority, set the **NextAction field** to
**01/07/2020** and mention that we will "reassess" the bug next year.  This
NextAction field is for Priority-3 bugs that are somehow less important than
other *priority-3* bugs.  These are bugs we’re sure no one on the team intends
to fix this year (e.g., really unimportant, or mostly unimportant and hard to
fix).  This label should be applied only when confident the whole team will
agree with you.  When searching the bugs database for things to do, I suggest
excluding bugs on this hotlist.)

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

The [timeline dashboard](http://go/uma-timeline) is your friend,
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

## Example request for chrome://omnibox data

NOTE: If you ask someone for chrome://omnibox data on a public bug, label the
bug with Restrict-View-Google so that any personal data from the reporter's
chrome://omnibox output is not made public. Do this *before* they respond.
As the original reporter, they should still have access to the bug even with the
restrict applied.

Example request:

> Please submit chrome://omnibox output for “\_\_\_”.  Check both the "show
> all details" and "show results per provider" boxes.  This will help us
> figure out what's going on.
>
> Feel free to paste the results in here unformatted; we’ll be able to
> decipher them.

## Commonly referenced bugs

* “I want the option to turn off inline autocomplete.” Dup against
  [crbug/91378](https://bugs.chromium.org/p/chromium/issues/detail?id=91378).

  * Try to understand the motivation of the user making the request.  Please
  ask the user for examples, with chrome://omnibox detail (see above), of times
  where the omnibox doesn’t do what they want.  Ideally we should be able make
  to make the omnibox smart enough that such a feature isn’t necessary.

* “I typed in something like go/foo and got redirected to a search results page
  instead.” See
  [this internal page](https://docs.google.com/document/d/140jmrHfC9BiNUbHEmUF4ajJ8Zpbc7qd5fjTBulH3I5g/edit#).
  Follow the guidelines there; generally ask about a message about profile
  corruption.  If so, the bug can be dupped against
  [crbug/665253](https://bugs.chromium.org/p/chromium/issues/detail?id=665253)

  * … and no “Did you mean?” infobar appears.  This is likely prerendering; see
  [crbug/247848](https://bugs.chromium.org/p/chromium/issues/detail?id=247848)

* “I want to disable suggestions from appearing entirely”.  Try to find out
  why.  Quote freely from
  [pkasting’s comment on this bug](https://bugs.chromium.org/p/chromium/issues/detail?id=702850#c16).

# References

[General Chromium bug triage guidelines](http://www.chromium.org/getting-involved/bug-triage)

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
