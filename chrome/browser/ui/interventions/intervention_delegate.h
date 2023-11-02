// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_DELEGATE_H_
#define CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_DELEGATE_H_

// An interface to handle user actions assocated to an intervention.
class InterventionDelegate {
 public:
  virtual void AcceptIntervention() = 0;
  virtual void DeclineIntervention() = 0;
  virtual void DeclineInterventionWithReload() = 0;

  // Called if the user declines the intervention in a sticky way. e.g. by
  // indicating they always want to decline the intervention on the site.
  virtual void DeclineInterventionSticky() = 0;

 protected:
  virtual ~InterventionDelegate() = default;
};

#endif  // CHROME_BROWSER_UI_INTERVENTIONS_INTERVENTION_DELEGATE_H_
