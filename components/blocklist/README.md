# Blocklist component #

The goal of the blocklist component is to provide various blocklists that allow
different policies for features to consume. Currently, the only implemented
blocklist is the opt out blocklist.

## Opt out blocklist ##
The opt out blocklist makes decisions based on user history actions. Each user
action is evaluated based on action type, time of the evaluation, host name of
the action (can be any string representation), and previous action history.

### Expected feature behavior ###
When a feature action is allowed, the feature may perform said action. After
performing the action, the user interaction should be determined to be an opt
out (the user did not like the action) or a non-opt out (the user was not
opposed to the action). The action, type, host name, and whether it was an opt
out should be reported back to the blocklist to build user action history.

For example, a feature may wish to show an InfoBar (or different types of
InfoBars) displaying information about the page a user is on. After querying the
opt out blocklist for action eligibility, an InfoBar may be allowed to be shown.
If it is shown, the user may interact with it in a number of ways. If the user
dismisses the InfoBar, that could be considered an opt out; if the user does
not dismiss the InfoBar that could be considered a non-opt out. All of the
information related to that action should be reported to the blocklist.

### Supported evaluation policies ###
In general, policies follow a specific form: the most recent _n_ actions are
evaluated, and if _t_ or more of them are opt outs the action will not be
allowed for a specified duration, _d_. For each policy, the feature specifies
whether the policy is enabled, and, if it is, the feature specifies _n_
(history), _t_ (threshold), and _d_ (duration) for each policy.

* Session policy: This policy only applies across all types and host names, but
is limited to actions that happened within the current session. The beginning of
a session is defined as the creation of the blocklist object or when the
blocklist is cleared (see below for details on clearing the blocklist).

* Persistent policy: This policy applies across all sessions, types and host
names.

* Host policy: This policy applies across all session and types, but keeps a
separate history for each host names. This rule allows specific host names to be
prevented from having an action performed for the specific user. When this
policy is enabled, the feature specifies a number of hosts that are stored in
memory (to limit memory footprint, query time, etc.)

* Type policy: This policy applies across all session and host names, but keeps
a separate history for each type. This rule allows specific types to be
prevented from having an action performed for the specific user. The feature
specifies a set of enabled types and versions for each type. This allows
removing past versions of types to be removed from the backing store.

### Clearing the blocklist ###
Because many actions should be cleared when user clears history, the opt out
blocklist allows clearing history in certain time ranges. All entries are
cleared for the specified time range, and the data in memory is repopulated
from the backing store.
